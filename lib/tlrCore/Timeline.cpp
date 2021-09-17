// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2021 Darby Johnston
// All rights reserved.

#include <tlrCore/Timeline.h>

#include <tlrCore/AVIOSystem.h>
#include <tlrCore/Assert.h>
#include <tlrCore/Error.h>
#include <tlrCore/File.h>
#include <tlrCore/LogSystem.h>
#include <tlrCore/Math.h>
#include <tlrCore/String.h>
#include <tlrCore/StringFormat.h>

#include <opentimelineio/clip.h>
#include <opentimelineio/externalReference.h>
#include <opentimelineio/imageSequenceReference.h>
#include <opentimelineio/linearTimeWarp.h>
#include <opentimelineio/stackAlgorithm.h>
#include <opentimelineio/timeline.h>
#include <opentimelineio/track.h>
#include <opentimelineio/transition.h>

#if defined(TLR_ENABLE_PYTHON)
#include <Python.h>
#endif

#include <atomic>
#include <array>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>

namespace tlr
{
    namespace timeline
    {
        std::vector<std::string> getExtensions()
        {
            //! \todo Get extensions for the Python adapters.
            return { ".otio" };
        }

        std::vector<otime::TimeRange> toRanges(std::vector<otime::RationalTime> frames)
        {
            std::vector<otime::TimeRange> out;
            if (!frames.empty())
            {
                std::sort(frames.begin(), frames.end());
                auto i = frames.begin();
                auto j = i;
                do
                {
                    auto k = j + 1;
                    if (k != frames.end() && (*k - *j).value() > 1)
                    {
                        out.push_back(otime::TimeRange::range_from_start_end_time_inclusive(*i, *j));
                        i = k;
                        j = k;
                    }
                    else if (k == frames.end())
                    {
                        out.push_back(otime::TimeRange::range_from_start_end_time_inclusive(*i, *j));
                        i = k;
                        j = k;
                    }
                    else
                    {
                        ++j;
                    }
                } while (j != frames.end());
            }
            return out;
        }

        const otio::Composable* getRoot(const otio::Composable* composable)
        {
            const otio::Composable* out = composable;
            for (; out->parent(); out = out->parent())
                ;
            return out;
        }

        TLR_ENUM_IMPL(
            Transition,
            "None",
            "Dissolve");
        TLR_ENUM_SERIALIZE_IMPL(Transition);

        Transition toTransition(const std::string& value)
        {
            Transition out = Transition::None;
            if (otio::Transition::Type::SMPTE_Dissolve == value)
            {
                out = Transition::Dissolve;
            }
            return out;
        }

        bool FrameLayer::operator == (const FrameLayer& other) const
        {
            return image == other.image &&
                imageB == other.imageB &&
                transition == other.transition &&
                transitionValue == other.transitionValue;
        }

        bool FrameLayer::operator != (const FrameLayer& other) const
        {
            return !(*this == other);
        }

        bool Frame::operator == (const Frame& other) const
        {
            return time == other.time &&
                layers == other.layers;
        }

        bool Frame::operator != (const Frame& other) const
        {
            return !(*this == other);
        }

        namespace
        {
#if defined(TLR_ENABLE_PYTHON)
            class PyObjectRef
            {
            public:
                PyObjectRef(PyObject* o) :
                    o(o)
                {
                    if (!o)
                    {
                        throw std::runtime_error("Python error");
                    }
                }

                ~PyObjectRef()
                {
                    Py_XDECREF(o);
                }

                PyObject* o = nullptr;

                operator PyObject* () const { return o; }
            };
#endif

            otio::SerializableObject::Retainer<otio::Timeline> readTimeline(
                const std::string& fileName,
                otio::ErrorStatus* errorStatus)
            {
                otio::SerializableObject::Retainer<otio::Timeline> out;
#if defined(TLR_ENABLE_PYTHON)
                Py_Initialize();
                try
                {
                    auto pyModule = PyObjectRef(PyImport_ImportModule("opentimelineio.adapters"));

                    auto pyReadFromFile = PyObjectRef(PyObject_GetAttrString(pyModule, "read_from_file"));
                    auto pyReadFromFileArgs = PyObjectRef(PyTuple_New(1));
                    auto pyReadFromFileArg = PyUnicode_FromStringAndSize(fileName.c_str(), fileName.size());
                    if (!pyReadFromFileArg)
                    {
                        throw std::runtime_error("Cannot create arg");
                    }
                    PyTuple_SetItem(pyReadFromFileArgs, 0, pyReadFromFileArg);
                    auto pyTimeline = PyObjectRef(PyObject_CallObject(pyReadFromFile, pyReadFromFileArgs));

                    auto pyToJSONString = PyObjectRef(PyObject_GetAttrString(pyTimeline, "to_json_string"));
                    auto pyJSONString = PyObjectRef(PyObject_CallObject(pyToJSONString, NULL));
                    out = otio::SerializableObject::Retainer<otio::Timeline>(
                        dynamic_cast<otio::Timeline*>(otio::Timeline::from_json_string(
                            PyUnicode_AsUTF8AndSize(pyJSONString, NULL),
                            errorStatus)));
                }
                catch (const std::exception& e)
                {
                    errorStatus->outcome = otio::ErrorStatus::Outcome::FILE_OPEN_FAILED;
                    errorStatus->details = e.what();
                }
                if (PyErr_Occurred())
                {
                    PyErr_Print();
                }
                Py_Finalize();
#else
                out = dynamic_cast<otio::Timeline*>(otio::Timeline::from_json_file(fileName, errorStatus));
#endif
                return out;
            }
        }

        struct Timeline::Private
        {
            file::Path fixPath(const file::Path&) const;
            file::Path getPath(const otio::ImageSequenceReference*) const;
            file::Path getPath(const otio::MediaReference*) const;

            bool getVideoInfo(const otio::Composable*, std::vector<imaging::Info>&) const;

            float transitionValue(double frame, double in, double out) const;

            void tick();
            void frameRequests();
            std::future<avio::VideoFrame> readVideoFrame(
                const otio::Track*,
                const otio::Clip*,
                const otime::RationalTime&,
                uint16_t videoLayer,
                const avio::Options&,
                const std::shared_ptr<imaging::Image>& = nullptr);
            void stopReaders();
            void delReaders();

            std::weak_ptr<core::Context> context;
            file::Path path;
            otio::SerializableObject::Retainer<otio::Timeline> otioTimeline;
            otime::RationalTime duration = time::invalidTime;
            otime::RationalTime globalStartTime = time::invalidTime;
            std::vector<imaging::Info> videoInfo;
            std::vector<otime::TimeRange> activeRanges;

            struct LayerData
            {
                LayerData() {};
                LayerData(LayerData&&) = default;

                std::future<avio::VideoFrame> image;
                std::future<avio::VideoFrame> imageB;
                Transition transition = Transition::None;
                float transitionValue = 0.F;
            };
            struct Request
            {
                Request() {};
                Request(Request&&) = default;

                otime::RationalTime time = time::invalidTime;
                uint16_t videoLayer = 0;
                std::shared_ptr<imaging::Image> image;
                std::promise<Frame> promise;

                std::vector<LayerData> layerData;
            };
            std::list<Request> requests;
            std::list<Request> requestsInProgress;
            size_t requestCount = 16;
            std::chrono::milliseconds requestTimeout = std::chrono::milliseconds(1);
            std::condition_variable requestCV;

            avio::Options ioOptions;
            struct Reader
            {
                std::shared_ptr<avio::IRead> read;
                avio::Info info;
            };
            std::map<const otio::Clip*, Reader> readers;
            std::list<std::shared_ptr<avio::IRead> > stoppedReaders;

            std::thread thread;
            std::mutex mutex;
            bool stopped = false;
            std::atomic<bool> running;

            std::chrono::steady_clock::time_point logTimer;
        };

        void Timeline::_init(
            const otio::SerializableObject::Retainer<otio::Timeline>& otioTimeline,
            const std::shared_ptr<core::Context>& context)
        {
            TLR_PRIVATE_P();

            p.context = context;
            p.otioTimeline = otioTimeline;

            // Get information about the timeline.
            otio::ErrorStatus errorStatus;
            p.duration = p.otioTimeline.value->duration(&errorStatus);
            if (errorStatus != otio::ErrorStatus::OK)
            {
                throw std::runtime_error(errorStatus.full_description);
            }
            if (p.otioTimeline.value->global_start_time().has_value())
            {
                p.globalStartTime = p.otioTimeline.value->global_start_time().value();
            }
            else
            {
                p.globalStartTime = otime::RationalTime(0, p.duration.rate());
            }
            p.getVideoInfo(p.otioTimeline.value->tracks(), p.videoInfo);

            // Create a new thread.
            p.running = true;
            p.thread = std::thread(
                [this]
                {
                    TLR_PRIVATE_P();

                    p.logTimer = std::chrono::steady_clock::now();
                    while (p.running)
                    {
                        p.tick();
                    }

                    std::list<Private::Request> requestsCleanup;
                    {
                        std::unique_lock<std::mutex> lock(p.mutex);
                        p.stopped = true;
                        while (!p.requests.empty())
                        {
                            requestsCleanup.push_back(std::move(p.requests.front()));
                            p.requests.pop_front();
                        }
                    }
                    while (!p.requestsInProgress.empty())
                    {
                        requestsCleanup.push_back(std::move(p.requestsInProgress.front()));
                        p.requestsInProgress.pop_front();
                    }
                    for (auto& request : requestsCleanup)
                    {
                        Frame frame;
                        frame.time = request.time;
                        for (auto& i : request.layerData)
                        {
                            FrameLayer layer;
                            if (i.image.valid())
                            {
                                layer.image = i.image.get().image;
                            }
                            if (i.imageB.valid())
                            {
                                layer.imageB = i.imageB.get().image;
                            }
                            layer.transition = i.transition;
                            layer.transitionValue = i.transitionValue;
                            frame.layers.push_back(layer);
                        }
                        request.promise.set_value(frame);
                    }
                });
        }

        Timeline::Timeline() :
            _p(new Private)
        {}

        Timeline::~Timeline()
        {
            TLR_PRIVATE_P();
            p.running = false;
            if (p.thread.joinable())
            {
                p.thread.join();
            }
        }

        std::shared_ptr<Timeline> Timeline::create(
            const otio::SerializableObject::Retainer<otio::Timeline>& timeline,
            const std::shared_ptr<core::Context>& context)
        {
            auto out = std::shared_ptr<Timeline>(new Timeline);
            out->_init(timeline, context);
            return out;
        }

        std::shared_ptr<Timeline> Timeline::create(
            const file::Path& path,
            const std::shared_ptr<core::Context>& context)
        {
            auto out = std::shared_ptr<Timeline>(new Timeline);
            otio::SerializableObject::Retainer<otio::Timeline> otioTimeline;
            std::string error;
            try
            {
                if (auto read = context->getSystem<avio::System>()->read(path))
                {
                    const auto info = read->getInfo().get();
                    otime::RationalTime globalStartTime(0.0, info.videoTimeRange.duration().rate());
                    auto otioClip = new otio::Clip;
                    otioClip->set_source_range(info.videoTimeRange);
                    if (avio::VideoType::Sequence == info.videoType && !path.getNumber().empty())
                    {
                        globalStartTime = info.videoTimeRange.start_time();
                        otioClip->set_media_reference(new otio::ImageSequenceReference(
                            path.getDirectory(),
                            path.getBaseName(),
                            path.getExtension(),
                            info.videoTimeRange.start_time().value(),
                            1,
                            info.videoTimeRange.duration().rate(),
                            path.getPadding()));
                    }
                    else
                    {
                        otioClip->set_media_reference(new otio::ExternalReference(path.get()));
                    }
                    auto otioTrack = new otio::Track();
                    otio::ErrorStatus errorStatus;
                    otioTrack->append_child(otioClip, &errorStatus);
                    if (errorStatus != otio::ErrorStatus::OK)
                    {
                        throw std::runtime_error("Cannot append child");
                    }
                    auto otioStack = new otio::Stack;
                    otioStack->append_child(otioTrack, &errorStatus);
                    if (errorStatus != otio::ErrorStatus::OK)
                    {
                        throw std::runtime_error("Cannot append child");
                    }
                    otioTimeline = new otio::Timeline;
                    otioTimeline->set_tracks(otioStack);
                    otioTimeline->set_global_start_time(globalStartTime);
                }
            }
            catch (const std::exception& e)
            {
                error = e.what();
            }
            if (!otioTimeline)
            {
                otio::ErrorStatus errorStatus;
                otioTimeline = readTimeline(path.get(), &errorStatus);
                if (errorStatus != otio::ErrorStatus::OK)
                {
                    throw std::runtime_error(errorStatus.full_description);
                }
            }
            if (!otioTimeline)
            {
                throw std::runtime_error(error);
            }
            out->_p->path = path;
            out->_init(otioTimeline, context);
            return out;
        }

        const std::weak_ptr<core::Context>& Timeline::getContext() const
        {
            return _p->context;
        }
        
        const otio::SerializableObject::Retainer<otio::Timeline>& Timeline::getTimeline() const
        {
            return _p->otioTimeline;
        }

        const file::Path& Timeline::getPath() const
        {
            return _p->path;
        }

        const otime::RationalTime& Timeline::getGlobalStartTime() const
        {
            return _p->globalStartTime;
        }

        const otime::RationalTime& Timeline::getDuration() const
        {
            return _p->duration;
        }

        const std::vector<imaging::Info>& Timeline::getVideoInfo() const
        {
            return _p->videoInfo;
        }

        std::future<Frame> Timeline::getFrame(
            const otime::RationalTime& time,
            uint16_t videoLayer,
            const std::shared_ptr<imaging::Image>& image)
        {
            TLR_PRIVATE_P();
            Private::Request request;
            request.time = time;
            request.videoLayer = videoLayer;
            request.image = image;
            auto future = request.promise.get_future();
            bool valid = false;
            {
                std::unique_lock<std::mutex> lock(p.mutex);
                if (!p.stopped)
                {
                    valid = true;
                    p.requests.push_back(std::move(request));
                }
            }
            if (valid)
            {
                p.requestCV.notify_one();
            }
            else
            {
                request.promise.set_value(Frame());
            }
            return future;
        }

        void Timeline::setActiveRanges(const std::vector<otime::TimeRange>& ranges)
        {
            _p->activeRanges = ranges;
        }

        void Timeline::cancelFrames()
        {
            TLR_PRIVATE_P();
            std::unique_lock<std::mutex> lock(p.mutex);
            p.requests.clear();
            for (auto& i : p.readers)
            {
                i.second.read->cancelVideoFrames();
            }
        }

        size_t Timeline::getRequestCount() const
        {
            TLR_PRIVATE_P();
            std::unique_lock<std::mutex> lock(p.mutex);
            return p.requestCount;
        }

        void Timeline::setRequestCount(size_t value)
        {
            TLR_PRIVATE_P();
            std::unique_lock<std::mutex> lock(p.mutex);
            p.requestCount = value;
        }

        std::chrono::milliseconds Timeline::getRequestTimeout() const
        {
            TLR_PRIVATE_P();
            std::unique_lock<std::mutex> lock(p.mutex);
            return p.requestTimeout;
        }

        void Timeline::setRequestTimeout(const std::chrono::milliseconds& value)
        {
            TLR_PRIVATE_P();
            std::unique_lock<std::mutex> lock(p.mutex);
            p.requestTimeout = value;
        }

        void Timeline::setIOOptions(const avio::Options& value)
        {
            TLR_PRIVATE_P();
            std::unique_lock<std::mutex> lock(p.mutex);
            p.ioOptions = value;
        }

        file::Path Timeline::Private::fixPath(const file::Path& path) const
        {
            std::string directory;
            if (!path.isAbsolute())
            {
                directory = this->path.getDirectory();
            }
            return file::Path(directory, path.get());
        }

        file::Path Timeline::Private::getPath(const otio::ImageSequenceReference* ref) const
        {
            std::stringstream ss;
            ss << ref->target_url_base() <<
                ref->name_prefix() <<
                std::setfill('0') << std::setw(ref->frame_zero_padding()) << ref->start_frame() <<
                ref->name_suffix();
            return file::Path(ss.str());
        }

        file::Path Timeline::Private::getPath(const otio::MediaReference* ref) const
        {
            file::Path out;
            if (auto externalRef = dynamic_cast<const otio::ExternalReference*>(ref))
            {
                //! \bug Handle URL parsing.
                out = file::Path(externalRef->target_url());
            }
            else if (auto imageSequenceRef = dynamic_cast<const otio::ImageSequenceReference*>(ref))
            {
                out = getPath(imageSequenceRef);
            }
            return fixPath(out);
        }

        bool Timeline::Private::getVideoInfo(const otio::Composable* composable, std::vector<imaging::Info>& videoInfo) const
        {
            if (auto clip = dynamic_cast<const otio::Clip*>(composable))
            {
                if (auto context = this->context.lock())
                {
                    // The first clip with video defines the image information
                    // for the timeline.
                    avio::Options options = ioOptions;
                    otio::ErrorStatus errorStatus;
                    options["SequenceIO/DefaultSpeed"] = string::Format("{0}").arg(clip->duration(&errorStatus).rate());
                    if (auto read = context->getSystem<avio::System>()->read(getPath(clip->media_reference()), options))
                    {
                        const auto info = read->getInfo().get();
                        if (!info.video.empty())
                        {
                            videoInfo = info.video;
                            return true;
                        }
                    }
                }
            }
            if (auto composition = dynamic_cast<const otio::Composition*>(composable))
            {
                for (const auto& child : composition->children())
                {
                    if (getVideoInfo(child, videoInfo))
                    {
                        return true;
                    }
                }
            }
            return false;
        }

        float Timeline::Private::transitionValue(double frame, double in, double out) const
        {
            return (frame - in) / (out - in);
        }

        void Timeline::Private::tick()
        {
            frameRequests();
            stopReaders();
            delReaders();

            // Logging.
            const auto now = std::chrono::steady_clock::now();
            const std::chrono::duration<float> diff = now - logTimer;
            if (diff.count() > 10.F)
            {
                logTimer = now;
                if (auto context = this->context.lock())
                {
                    const std::string id = string::Format("tlr::timeline::Timeline {0}").arg(this);
                    size_t requestsSize = 0;
                    size_t requestCount = 0;
                    {
                        std::unique_lock<std::mutex> lock(mutex);
                        requestsSize = requests.size();
                        requestCount = this->requestCount;
                    }
                    auto logSystem = context->getLogSystem();
                    logSystem->print(id, string::Format("path: {0}, requests: {1}, in progress: {2}, count: {3}, readers: {4}").
                        arg(path.get()).
                        arg(requestsSize).
                        arg(requestsInProgress.size()).
                        arg(requestCount).
                        arg(readers.size()));
                }
            }
        }

        void Timeline::Private::frameRequests()
        {
            // Gather requests.
            std::list<Request> newRequests;
            avio::Options ioOptions;
            {
                std::unique_lock<std::mutex> lock(mutex);
                requestCV.wait_for(
                    lock,
                    requestTimeout,
                    [this]
                    {
                        return !requests.empty() || !requestsInProgress.empty();
                    });
                while (!requests.empty() &&
                    (requestsInProgress.size() + newRequests.size()) < requestCount)
                {
                    newRequests.push_back(std::move(requests.front()));
                    requests.pop_front();
                }
                ioOptions = this->ioOptions;
            }

            // Traverse the timeline for new requests.
            for (auto& request : newRequests)
            {
                try
                {
                    for (const auto& j : otioTimeline->tracks()->children())
                    {
                        const auto otioTrack = dynamic_cast<otio::Track*>(j.value);
                        if (otioTrack && otio::Track::Kind::video == otioTrack->kind())
                        {
                            for (const auto& k : otioTrack->children())
                            {
                                if (const auto otioItem = dynamic_cast<otio::Item*>(k.value))
                                {
                                    otio::ErrorStatus errorStatus;
                                    const auto rangeOpt = otioItem->trimmed_range_in_parent(&errorStatus);
                                    if (rangeOpt.has_value())
                                    {
                                        const auto& range = rangeOpt.value();
                                        const auto time = request.time - globalStartTime;
                                        if (range.contains(time))
                                        {
                                            LayerData data;
                                            if (const auto otioClip = dynamic_cast<otio::Clip*>(otioItem))
                                            {
                                                data.image = readVideoFrame(otioTrack, otioClip, time, request.videoLayer, ioOptions, request.image);
                                            }
                                            const auto neighbors = otioTrack->neighbors_of(otioItem, &errorStatus);
                                            if (auto otioTransition = dynamic_cast<otio::Transition*>(neighbors.second.value))
                                            {
                                                if (time > range.end_time_inclusive() - otioTransition->in_offset())
                                                {
                                                    data.transition = toTransition(otioTransition->transition_type());
                                                    data.transitionValue = transitionValue(
                                                        time.value(),
                                                        range.end_time_inclusive().value() - otioTransition->in_offset().value(),
                                                        range.end_time_inclusive().value() + otioTransition->out_offset().value() + 1.0);
                                                    const auto transitionNeighbors = otioTrack->neighbors_of(otioTransition, &errorStatus);
                                                    if (const auto otioClipB = dynamic_cast<otio::Clip*>(transitionNeighbors.second.value))
                                                    {
                                                        data.imageB = readVideoFrame(otioTrack, otioClipB, time, request.videoLayer, ioOptions);
                                                    }
                                                }
                                            }
                                            if (auto otioTransition = dynamic_cast<otio::Transition*>(neighbors.first.value))
                                            {
                                                if (time < range.start_time() + otioTransition->out_offset())
                                                {
                                                    std::swap(data.image, data.imageB);
                                                    data.transition = toTransition(otioTransition->transition_type());
                                                    data.transitionValue = transitionValue(
                                                        time.value(),
                                                        range.start_time().value() - otioTransition->in_offset().value() - 1.0,
                                                        range.start_time().value() + otioTransition->out_offset().value());
                                                    const auto transitionNeighbors = otioTrack->neighbors_of(otioTransition, &errorStatus);
                                                    if (const auto otioClipB = dynamic_cast<otio::Clip*>(transitionNeighbors.first.value))
                                                    {
                                                        data.image = readVideoFrame(otioTrack, otioClipB, time, request.videoLayer, ioOptions);
                                                    }
                                                }
                                            }
                                            request.layerData.push_back(std::move(data));
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                catch (const std::exception&)
                {
                    //! \todo How should this be handled?
                }

                requestsInProgress.push_back(std::move(request));
            }

            // Check for finished requests.
            auto requestIt = requestsInProgress.begin();
            while (requestIt != requestsInProgress.end())
            {
                bool valid = true;
                for (auto& i : requestIt->layerData)
                {
                    if (i.image.valid())
                    {
                        valid &= i.image.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
                    }
                    if (i.imageB.valid())
                    {
                        valid &= i.imageB.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
                    }
                }
                if (valid)
                {
                    Frame frame;
                    frame.time = requestIt->time;
                    try
                    {
                        for (auto& j : requestIt->layerData)
                        {
                            FrameLayer layer;
                            if (j.image.valid())
                            {
                                layer.image = j.image.get().image;
                            }
                            if (j.imageB.valid())
                            {
                                layer.imageB = j.imageB.get().image;
                            }
                            layer.transition = j.transition;
                            layer.transitionValue = j.transitionValue;
                            frame.layers.push_back(layer);
                        }
                    }
                    catch (const std::exception&)
                    {
                        //! \todo How should this be handled?
                    }
                    requestIt->promise.set_value(frame);
                    requestIt = requestsInProgress.erase(requestIt);
                    continue;
                }
                ++requestIt;
            }
        }

        std::future<avio::VideoFrame> Timeline::Private::readVideoFrame(
            const otio::Track* track,
            const otio::Clip* clip,
            const otime::RationalTime& time,
            uint16_t videoLayer,
            const avio::Options& ioOptions,
            const std::shared_ptr<imaging::Image>& image)
        {
            std::future<avio::VideoFrame> out;
            if (auto context = this->context.lock())
            {
                // Get the clip time transform.
                //
                //! \bug This only applies time transform at the clip level.
                otio::TimeTransform timeTransform;
                for (const auto& effect : clip->effects())
                {
                    if (auto linearTimeWarp = dynamic_cast<otio::LinearTimeWarp*>(effect.value))
                    {
                        timeTransform = otio::TimeTransform(otime::RationalTime(), linearTimeWarp->time_scalar()).applied_to(timeTransform);
                    }
                }

                // Get the clip start time taking transitions into account.
                otime::RationalTime startTime;
                otio::ErrorStatus errorStatus;
                const auto range = clip->trimmed_range(&errorStatus);
                startTime = range.start_time();
                const auto neighbors = track->neighbors_of(clip, &errorStatus);
                if (auto transition = dynamic_cast<const otio::Transition*>(neighbors.first.value))
                {
                    startTime -= transition->in_offset();
                }
                
                // Get the frame time.
                const auto clipTime = track->transformed_time(time, clip, &errorStatus);
                auto frameTime = startTime + timeTransform.applied_to(clipTime - startTime);

                // Read the frame.
                const auto ioSystem = context->getSystem<avio::System>();
                const auto j = readers.find(clip);
                if (j != readers.end())
                {
                    const auto readTime = frameTime.rescaled_to(j->second.info.videoTimeRange.duration().rate());
                    const auto floorTime = otime::RationalTime(floor(readTime.value()), readTime.rate());
                    out = j->second.read->readVideoFrame(floorTime, videoLayer, image);
                }
                else
                {
                    const file::Path path = getPath(clip->media_reference());
                    avio::Options options = ioOptions;
                    options["SequenceIO/DefaultSpeed"] = string::Format("{0}").arg(duration.rate());
                    auto read = ioSystem->read(path, options);
                    avio::Info info;
                    if (read)
                    {
                        info = read->getInfo().get();
                    }
                    if (read && !info.video.empty())
                    {
                        context->log("tlr::timeline::Timeline", this->path.get() + ": Read: " + path.get());
                        Reader reader;
                        reader.read = read;
                        reader.info = info;
                        const auto readTime = frameTime.rescaled_to(info.videoTimeRange.duration().rate());
                        const auto floorTime = otime::RationalTime(floor(readTime.value()), readTime.rate());
                        out = read->readVideoFrame(floorTime, videoLayer, image);
                        readers[clip] = std::move(reader);
                    }
                }
            }
            return out;
        }

        void Timeline::Private::stopReaders()
        {
            auto i = readers.begin();
            while (i != readers.end())
            {
                const auto clip = i->first;

                otio::ErrorStatus errorStatus;
                const auto trimmedRange = clip->trimmed_range(&errorStatus);
                const auto ancestor = dynamic_cast<const otio::Item*>(getRoot(clip));
                const auto clipRange = i->first->transformed_time_range(trimmedRange, ancestor, &errorStatus);
                auto startTime = clipRange.start_time();
                auto endTime = startTime + clipRange.duration();
                const auto track = getParent<otio::Track>(clip);
                const auto neighbors = track->neighbors_of(clip, &errorStatus);
                if (auto transition = dynamic_cast<const otio::Transition*>(neighbors.first.value))
                {
                    startTime -= transition->in_offset();
                }
                if (auto transition = dynamic_cast<const otio::Transition*>(neighbors.second.value))
                {
                    endTime += transition->out_offset();
                }
                const auto range = otime::TimeRange::range_from_start_end_time(globalStartTime + startTime, globalStartTime + endTime);

                bool del = true;
                for (const auto& activeRange : activeRanges)
                {
                    if (range.intersects(activeRange))
                    {
                        del = false;
                        break;
                    }
                }
                if (del && !i->second.read->hasVideoFrames())
                {
                    if (auto context = this->context.lock())
                    {
                        context->log("tlr::timeline::Timeline", path.get() + ": Stop: " + i->second.read->getPath().get());
                    }
                    auto read = i->second.read;
                    read->stop();
                    stoppedReaders.push_back(read);
                    i = readers.erase(i);
                }
                else
                {
                    ++i;
                }
            }
        }

        void Timeline::Private::delReaders()
        {
            auto i = stoppedReaders.begin();
            while (i != stoppedReaders.end())
            {
                if ((*i)->hasStopped())
                {
                    if (auto context = this->context.lock())
                    {
                        context->log("tlr::timeline::Timeline", path.get() + ": Delete: " + (*i)->getPath().get());
                    }
                    i = stoppedReaders.erase(i);
                }
                else
                {
                    ++i;
                }
            }
        }
    }
}
