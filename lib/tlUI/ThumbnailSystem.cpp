// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2021-2024 Darby Johnston
// All rights reserved.

#include <tlUI/ThumbnailSystem.h>

#include <tlTimelineGL/Render.h>

#include <tlIO/System.h>

#include <tlGL/GL.h>
#include <tlGL/GLFWWindow.h>
#include <tlGL/OffscreenBuffer.h>

#include <tlCore/AudioResample.h>
#include <tlCore/LRUCache.h>
#include <tlCore/StringFormat.h>

#include <sstream>

namespace tl
{
    namespace ui
    {
        namespace
        {
            const size_t infoRequestsMax = 3;
            const size_t thumbnailRequestsMax = 3;
            const size_t waveformRequestsMax = 3;
        }

        struct ThumbnailCache::Private
        {
            size_t max = 1000;
            memory::LRUCache<std::string, io::Info> info;
            memory::LRUCache<std::string, std::shared_ptr<image::Image> > thumbnails;
            memory::LRUCache<std::string, std::shared_ptr<geom::TriangleMesh2> > waveforms;
            std::mutex mutex;
        };

        void ThumbnailCache::_init(const std::shared_ptr<system::Context>& context)
        {
            _maxUpdate();
        }

        ThumbnailCache::ThumbnailCache() :
            _p(new Private)
        {}

        ThumbnailCache::~ThumbnailCache()
        {}

        std::shared_ptr<ThumbnailCache> ThumbnailCache::create(
            const std::shared_ptr<system::Context>& context)
        {
            auto out = std::shared_ptr<ThumbnailCache>(new ThumbnailCache);
            out->_init(context);
            return out;
        }

        size_t ThumbnailCache::getMax() const
        {
            return _p->max;
        }

        void ThumbnailCache::setMax(size_t value)
        {
            TLRENDER_P();
            if (value == p.max)
                return;
            p.max = value;
            _maxUpdate();
        }

        size_t ThumbnailCache::getSize() const
        {
            TLRENDER_P();
            std::unique_lock<std::mutex> lock(p.mutex);
            return p.info.getSize() + p.thumbnails.getSize() + p.waveforms.getSize();
        }

        float ThumbnailCache::getPercentage() const
        {
            TLRENDER_P();
            std::unique_lock<std::mutex> lock(p.mutex);
            return
                (p.info.getSize() + p.thumbnails.getSize() + p.waveforms.getSize()) /
                static_cast<float>(p.info.getMax() + p.thumbnails.getMax() + p.waveforms.getMax()) * 100.F;
        }

        std::string ThumbnailCache::getInfoKey(
            const file::Path& path,
            const io::Options& options)
        {
            std::vector<std::string> s;
            s.push_back(path.get());
            for (const auto& i : options)
            {
                s.push_back(string::Format("{0}:{1}").arg(i.first).arg(i.second));
            }
            return string::join(s, ';');
        }

        void ThumbnailCache::addInfo(const std::string& key, const io::Info& info)
        {
            TLRENDER_P();
            std::unique_lock<std::mutex> lock(p.mutex);
            p.info.add(key, info);
        }

        bool ThumbnailCache::containsInfo(const std::string& key)
        {
            TLRENDER_P();
            std::unique_lock<std::mutex> lock(p.mutex);
            return p.info.contains(key);
        }

        bool ThumbnailCache::getInfo(const std::string& key, io::Info& info) const
        {
            TLRENDER_P();
            std::unique_lock<std::mutex> lock(p.mutex);
            return p.info.get(key, info);
        }

        std::string ThumbnailCache::getThumbnailKey(
            int height,
            const file::Path& path,
            const otime::RationalTime& time,
            const io::Options& options)
        {
            std::vector<std::string> s;
            s.push_back(string::Format("{0}").arg(height));
            s.push_back(path.get());
            s.push_back(string::Format("{0}").arg(time));
            for (const auto& i : options)
            {
                s.push_back(string::Format("{0}:{1}").arg(i.first).arg(i.second));
            }
            return string::join(s, ';');
        }

        void ThumbnailCache::addThumbnail(
            const std::string& key,
            const std::shared_ptr<image::Image>& thumbnail)
        {
            TLRENDER_P();
            std::unique_lock<std::mutex> lock(p.mutex);
            p.thumbnails.add(key, thumbnail);
        }

        bool ThumbnailCache::containsThumbnail(const std::string& key)
        {
            TLRENDER_P();
            std::unique_lock<std::mutex> lock(p.mutex);
            return p.thumbnails.contains(key);
        }

        bool ThumbnailCache::getThumbnail(
            const std::string& key,
            std::shared_ptr<image::Image>& thumbnail) const
        {
            TLRENDER_P();
            std::unique_lock<std::mutex> lock(p.mutex);
            return p.thumbnails.get(key, thumbnail);
        }

        std::string ThumbnailCache::getWaveformKey(
            const math::Size2i& size,
            const file::Path& path,
            const otime::TimeRange& timeRange,
            const io::Options& options)
        {
            std::vector<std::string> s;
            s.push_back(string::Format("{0}").arg(size));
            s.push_back(path.get());
            s.push_back(string::Format("{0}").arg(timeRange));
            for (const auto& i : options)
            {
                s.push_back(string::Format("{0}:{1}").arg(i.first).arg(i.second));
            }
            return string::join(s, ';');
        }

        void ThumbnailCache::addWaveform(
            const std::string& key,
            const std::shared_ptr<geom::TriangleMesh2>& waveform)
        {
            TLRENDER_P();
            std::unique_lock<std::mutex> lock(p.mutex);
            p.waveforms.add(key, waveform);
        }

        bool ThumbnailCache::containsWaveform(const std::string& key)
        {
            TLRENDER_P();
            std::unique_lock<std::mutex> lock(p.mutex);
            return p.waveforms.contains(key);
        }

        bool ThumbnailCache::getWaveform(
            const std::string& key,
            std::shared_ptr<geom::TriangleMesh2>& waveform) const
        {
            TLRENDER_P();
            std::unique_lock<std::mutex> lock(p.mutex);
            return p.waveforms.get(key, waveform);
        }

        void ThumbnailCache::_maxUpdate()
        {
            TLRENDER_P();
            std::unique_lock<std::mutex> lock(p.mutex);
            p.info.setMax(p.max);
            p.thumbnails.setMax(p.max);
            p.waveforms.setMax(p.max);
        }

        struct ThumbnailGenerator::Private
        {
            std::weak_ptr<system::Context> context;
            std::shared_ptr<ThumbnailCache> cache;
            std::shared_ptr<gl::GLFWWindow> window;
            uint64_t requestId = 0;

            struct InfoRequest
            {
                uint64_t id = 0;
                file::Path path;
                std::vector<file::MemoryRead> memoryRead;
                io::Options options;
                std::promise<io::Info> promise;
            };

            struct ThumbnailRequest
            {
                uint64_t id = 0;
                file::Path path;
                std::vector<file::MemoryRead> memoryRead;
                int height = 0;
                otime::RationalTime time = time::invalidTime;
                io::Options options;
                std::promise<std::shared_ptr<image::Image> > promise;
            };

            struct WaveformRequest
            {
                uint64_t id = 0;
                file::Path path;
                std::vector<file::MemoryRead> memoryRead;
                math::Size2i size;
                otime::TimeRange timeRange = time::invalidTimeRange;
                io::Options options;
                std::promise<std::shared_ptr<geom::TriangleMesh2> > promise;
            };
            
            struct Mutex
            {
                std::list<std::shared_ptr<InfoRequest> > infoRequests;
                std::list<std::shared_ptr<ThumbnailRequest> > thumbnailRequests;
                std::list<std::shared_ptr<WaveformRequest> > waveformRequests;
                bool stopped = false;
                std::mutex mutex;
            };
            Mutex mutex;
            
            struct Thread
            {
                memory::LRUCache<std::string, std::shared_ptr<io::IRead> > ioCache;
                std::chrono::steady_clock::time_point logTimer;
                std::condition_variable cv;
                std::thread thread;
                std::atomic<bool> running;
            };
            Thread thread;
        };

        void ThumbnailGenerator::_init(
            const std::shared_ptr<system::Context>& context)
        {
            TLRENDER_P();
            
            p.context = context;

            p.cache = context->getSystem<ThumbnailSystem>()->getCache();

            p.window = gl::GLFWWindow::create(
                "tl::ui::ThumbnailGenerator",
                math::Size2i(1, 1),
                context,
                static_cast<int>(gl::GLFWWindowOptions::None));

            p.thread.ioCache.setMax(1000);
            p.thread.running = true;
            p.thread.thread = std::thread(
                [this]
                {
                    TLRENDER_P();
                    try
                    {
                        p.window->makeCurrent();
                    }
                    catch (const std::exception& e)
                    {
                        if (auto context = p.context.lock())
                        {
                            context->log(
                                "tl::ui::ThumbnailGenerator",
                                string::Format("Cannot make the OpenGL context current: {0}").
                                    arg(e.what()),
                                log::Type::Error);
                        }
                    }
                    _run();
                    {
                        std::unique_lock<std::mutex> lock(p.mutex.mutex);
                        p.mutex.stopped = true;
                    }
                    p.window->doneCurrent();
                    _cancelRequests();
                });
        }

        ThumbnailGenerator::ThumbnailGenerator() :
            _p(new Private)
        {}

        ThumbnailGenerator::~ThumbnailGenerator()
        {
            TLRENDER_P();
            p.thread.running = false;
            if (p.thread.thread.joinable())
            {
                p.thread.thread.join();
            }
        }

        std::shared_ptr<ThumbnailGenerator> ThumbnailGenerator::create(
            const std::shared_ptr<system::Context>& context)
        {
            auto out = std::shared_ptr<ThumbnailGenerator>(new ThumbnailGenerator);
            out->_init(context);
            return out;
        }

        InfoRequest ThumbnailGenerator::getInfo(
            const file::Path& path,
            const io::Options& options)
        {
            return getInfo(path, {}, options);
        }

        InfoRequest ThumbnailGenerator::getInfo(
            const file::Path& path,
            const std::vector<file::MemoryRead>& memoryRead,
            const io::Options& options)
        {
            TLRENDER_P();
            (p.requestId)++;
            auto request = std::make_shared<Private::InfoRequest>();
            request->id = p.requestId;
            request->path = path;
            request->memoryRead = memoryRead;
            request->options = options;
            InfoRequest out;
            out.id = p.requestId;
            out.future = request->promise.get_future();
            bool valid = false;
            {
                std::unique_lock<std::mutex> lock(p.mutex.mutex);
                if (!p.mutex.stopped)
                {
                    valid = true;
                    p.mutex.infoRequests.push_back(request);
                }
            }
            if (valid)
            {
                p.thread.cv.notify_one();
            }
            else
            {
                request->promise.set_value(io::Info());
            }
            return out;
        }

        ThumbnailRequest ThumbnailGenerator::getThumbnail(
            const file::Path& path,
            int height,
            const otime::RationalTime& time,
            const io::Options& options)
        {
            return getThumbnail(path, {}, height, time, options);
        }

        ThumbnailRequest ThumbnailGenerator::getThumbnail(
            const file::Path& path,
            const std::vector<file::MemoryRead>& memoryRead,
            int height,
            const otime::RationalTime& time,
            const io::Options& options)
        {
            TLRENDER_P();
            (p.requestId)++;
            auto request = std::make_shared<Private::ThumbnailRequest>();
            request->id = p.requestId;
            request->path = path;
            request->memoryRead = memoryRead;
            request->height = height;
            request->time = time;
            request->options = options;
            ThumbnailRequest out;
            out.id = p.requestId;
            out.height = height;
            out.time = time;
            out.future = request->promise.get_future();
            bool valid = false;
            {
                std::unique_lock<std::mutex> lock(p.mutex.mutex);
                if (!p.mutex.stopped)
                {
                    valid = true;
                    p.mutex.thumbnailRequests.push_back(request);
                }
            }
            if (valid)
            {
                p.thread.cv.notify_one();
            }
            else
            {
                request->promise.set_value(nullptr);
            }
            return out;
        }

        WaveformRequest ThumbnailGenerator::getWaveform(
            const file::Path& path,
            const math::Size2i& size,
            const otime::TimeRange& range,
            const io::Options& options)
        {
            return getWaveform(path, {}, size, range, options);
        }

        WaveformRequest ThumbnailGenerator::getWaveform(
            const file::Path& path,
            const std::vector<file::MemoryRead>& memoryRead,
            const math::Size2i& size,
            const otime::TimeRange& timeRange,
            const io::Options& options)
        {
            TLRENDER_P();
            (p.requestId)++;
            auto request = std::make_shared<Private::WaveformRequest>();
            request->id = p.requestId;
            request->path = path;
            request->memoryRead = memoryRead;
            request->size = size;
            request->timeRange = timeRange;
            request->options = options;
            WaveformRequest out;
            out.id = p.requestId;
            out.size = size;
            out.timeRange = timeRange;
            out.future = request->promise.get_future();
            bool valid = false;
            {
                std::unique_lock<std::mutex> lock(p.mutex.mutex);
                if (!p.mutex.stopped)
                {
                    valid = true;
                    p.mutex.waveformRequests.push_back(request);
                }
            }
            if (valid)
            {
                p.thread.cv.notify_one();
            }
            else
            {
                request->promise.set_value(nullptr);
            }
            return out;
        }

        void ThumbnailGenerator::cancelRequests(std::vector<uint64_t> ids)
        {
            TLRENDER_P();
            std::unique_lock<std::mutex> lock(p.mutex.mutex);
            auto infoRequest = p.mutex.infoRequests.begin();
            while (infoRequest != p.mutex.infoRequests.end())
            {
                const auto id = std::find(ids.begin(), ids.end(), (*infoRequest)->id);
                if (id != ids.end())
                {
                    infoRequest = p.mutex.infoRequests.erase(infoRequest);
                    ids.erase(id);
                }
                else
                {
                    ++infoRequest;
                }
            }
            auto thumbnailRequest = p.mutex.thumbnailRequests.begin();
            while (thumbnailRequest != p.mutex.thumbnailRequests.end())
            {
                const auto id = std::find(ids.begin(), ids.end(), (*thumbnailRequest)->id);
                if (id != ids.end())
                {
                    thumbnailRequest = p.mutex.thumbnailRequests.erase(thumbnailRequest);
                    ids.erase(id);
                }
                else
                {
                    ++thumbnailRequest;
                }
            }
            auto waveformRequest = p.mutex.waveformRequests.begin();
            while (waveformRequest != p.mutex.waveformRequests.end())
            {
                const auto id = std::find(ids.begin(), ids.end(), (*waveformRequest)->id);
                if (id != ids.end())
                {
                    waveformRequest = p.mutex.waveformRequests.erase(waveformRequest);
                    ids.erase(id);
                }
                else
                {
                    ++waveformRequest;
                }
            }
        }

        namespace
        {
            std::shared_ptr<geom::TriangleMesh2> audioMesh(
                const std::shared_ptr<audio::Audio>& audio,
                const math::Size2i& size)
            {
                auto out = std::shared_ptr<geom::TriangleMesh2>(new geom::TriangleMesh2);
                const auto& info = audio->getInfo();
                const size_t sampleCount = audio->getSampleCount();
                if (sampleCount > 0)
                {
                    switch (info.dataType)
                    {
                    case audio::DataType::F32:
                    {
                        const audio::F32_T* data = reinterpret_cast<const audio::F32_T*>(
                            audio->getData());
                        for (int x = 0; x < size.w; ++x)
                        {
                            const int x0 = std::min(
                                static_cast<size_t>((x + 0) / static_cast<double>(size.w - 1) * (sampleCount - 1)),
                                sampleCount - 1);
                            const int x1 = std::min(
                                static_cast<size_t>((x + 1) / static_cast<double>(size.w - 1) * (sampleCount - 1)),
                                sampleCount - 1);
                            //std::cout << x << ": " << x0 << " " << x1 << std::endl;
                            audio::F32_T min = 0.F;
                            audio::F32_T max = 0.F;
                            if (x0 < x1)
                            {
                                min = audio::F32Range.getMax();
                                max = audio::F32Range.getMin();
                                for (int i = x0; i < x1; ++i)
                                {
                                    const audio::F32_T v = *(data + i * info.channelCount);
                                    min = std::min(min, v);
                                    max = std::max(max, v);
                                }
                            }
                            const int h2 = size.h / 2;
                            const math::Box2i box(
                                math::Vector2i(
                                    x,
                                    h2 - h2 * max),
                                math::Vector2i(
                                    x + 1,
                                    h2 - h2 * min));
                            if (box.isValid())
                            {
                                const size_t j = 1 + out->v.size();
                                out->v.push_back(math::Vector2f(box.x(), box.y()));
                                out->v.push_back(math::Vector2f(box.x() + box.w(), box.y()));
                                out->v.push_back(math::Vector2f(box.x() + box.w(), box.y() + box.h()));
                                out->v.push_back(math::Vector2f(box.x(), box.y() + box.h()));
                                out->triangles.push_back(geom::Triangle2({ j + 0, j + 1, j + 2 }));
                                out->triangles.push_back(geom::Triangle2({ j + 2, j + 3, j + 0 }));
                            }
                        }
                        break;
                    }
                    default: break;
                    }
                }
                return out;
            }

            std::shared_ptr<image::Image> audioImage(
                const std::shared_ptr<audio::Audio>& audio,
                const math::Size2i& size)
            {
                auto out = image::Image::create(size.w, size.h, image::PixelType::L_U8);
                const auto& info = audio->getInfo();
                const size_t sampleCount = audio->getSampleCount();
                if (sampleCount > 0)
                {
                    switch (info.dataType)
                    {
                    case audio::DataType::F32:
                    {
                        const audio::F32_T* data = reinterpret_cast<const audio::F32_T*>(
                            audio->getData());
                        for (int x = 0; x < size.w; ++x)
                        {
                            const int x0 = std::min(
                                static_cast<size_t>((x + 0) / static_cast<double>(size.w - 1) * (sampleCount - 1)),
                                sampleCount - 1);
                            const int x1 = std::min(
                                static_cast<size_t>((x + 1) / static_cast<double>(size.w - 1) * (sampleCount - 1)),
                                sampleCount - 1);
                            //std::cout << x << ": " << x0 << " " << x1 << std::endl;
                            audio::F32_T min = 0.F;
                            audio::F32_T max = 0.F;
                            if (x0 < x1)
                            {
                                min = audio::F32Range.getMax();
                                max = audio::F32Range.getMin();
                                for (int i = x0; i < x1; ++i)
                                {
                                    const audio::F32_T v = *(data + i * info.channelCount);
                                    min = std::min(min, v);
                                    max = std::max(max, v);
                                }
                            }
                            uint8_t* p = out->getData() + x;
                            for (int y = 0; y < size.h; ++y)
                            {
                                const float v = y / static_cast<float>(size.h - 1) * 2.F - 1.F;
                                *p = (v > min && v < max) ? 255 : 0;
                                p += size.w;
                            }
                        }
                        break;
                    }
                    default: break;
                    }
                }
                return out;
            }
        }
        
        void ThumbnailGenerator::_run()
        {
            TLRENDER_P();
            std::shared_ptr<timeline_gl::Render> render;
            if (auto context = p.context.lock())
            {
                render = timeline_gl::Render::create(context);
            }
            std::shared_ptr<gl::OffscreenBuffer> buffer;
            io::Options ioOptions;
            p.thread.logTimer = std::chrono::steady_clock::now();
            while (p.thread.running)
            {
                // Check requests.
                size_t infoRequestsSize = 0;
                size_t thumbnailRequestsSize = 0;
                size_t waveformRequestsSize = 0;
                std::list<std::shared_ptr<Private::InfoRequest> > infoRequests;
                std::list<std::shared_ptr<Private::ThumbnailRequest> > thumbnailRequests;
                std::list<std::shared_ptr<Private::WaveformRequest> > waveformRequests;
                {
                    std::unique_lock<std::mutex> lock(p.mutex.mutex);
                    if (p.thread.cv.wait_for(
                        lock,
                        std::chrono::milliseconds(5),
                        [this]
                        {
                            return
                                !_p->mutex.infoRequests.empty() ||
                                !_p->mutex.thumbnailRequests.empty() ||
                                !_p->mutex.waveformRequests.empty();
                        }))
                    {
                        infoRequestsSize = p.mutex.infoRequests.size();
                        thumbnailRequestsSize = p.mutex.thumbnailRequests.size();
                        waveformRequestsSize = p.mutex.waveformRequests.size();
                        while (!p.mutex.infoRequests.empty() &&
                            infoRequests.size() < infoRequestsMax)
                        {
                            infoRequests.push_back(p.mutex.infoRequests.front());
                            p.mutex.infoRequests.pop_front();
                        }
                        if (!p.mutex.thumbnailRequests.empty() &&
                            thumbnailRequests.size() < thumbnailRequestsMax)
                        {
                            thumbnailRequests.push_back(p.mutex.thumbnailRequests.front());
                            p.mutex.thumbnailRequests.pop_front();
                        }
                        if (!p.mutex.waveformRequests.empty() &&
                            waveformRequests.size() < waveformRequestsMax)
                        {
                            waveformRequests.push_back(p.mutex.waveformRequests.front());
                            p.mutex.waveformRequests.pop_front();
                        }
                    }
                }
                /*if (infoRequestsSize || thumbnailRequestsSize || waveformRequestsSize)
                {
                    std::cout << "info requests: " << infoRequestsSize << std::endl;
                    std::cout << "thumbnail requests: " << thumbnailRequestsSize << std::endl;
                    std::cout << "waveform requests: " << waveformRequestsSize << std::endl;
                }*/
                                
                // Handle information requests.
                for (const auto& infoRequest : infoRequests)
                {
                    io::Info info;
                    const std::string key = ThumbnailCache::getInfoKey(
                        infoRequest->path,
                        infoRequest->options);
                    if (!p.cache->getInfo(key, info))
                    {
                        const std::string& fileName = infoRequest->path.get();
                        //std::cout << "info request: " << infoRequest->path.get() << std::endl;
                        std::shared_ptr<io::IRead> read;
                        if (!p.thread.ioCache.get(fileName, read))
                        {
                            if (auto context = p.context.lock())
                            {
                                auto ioSystem = context->getSystem<io::System>();
                                try
                                {
                                    read = ioSystem->read(
                                        infoRequest->path,
                                        infoRequest->memoryRead,
                                        infoRequest->options);
                                    p.thread.ioCache.add(fileName, read);
                                }
                                catch (const std::exception&)
                                {}
                            }
                        }
                        if (read)
                        {
                            info = read->getInfo().get();
                        }
                        p.cache->addInfo(key, info);
                    }
                    infoRequest->promise.set_value(info);
                }
                
                // Handle thumbnail requests.
                for (const auto& request : thumbnailRequests)
                {
                    std::shared_ptr<image::Image> image;
                    const std::string key = ThumbnailCache::getThumbnailKey(
                        request->height,
                        request->path,
                        request->time,
                        request->options);
                    if (!p.cache->getThumbnail(key, image))
                    {
                        try
                        {
                            const std::string& fileName = request->path.get();
                            //std::cout << "thumbnail request: " << fileName << " " <<
                            //    request->time << std::endl;
                            std::shared_ptr<io::IRead> read;
                            if (!p.thread.ioCache.get(fileName, read))
                            {
                                if (auto context = p.context.lock())
                                {
                                    auto ioSystem = context->getSystem<io::System>();
                                    try
                                    {
                                        read = ioSystem->read(
                                            request->path,
                                            request->memoryRead,
                                            request->options);
                                        p.thread.ioCache.add(fileName, read);
                                    }
                                    catch (const std::exception&)
                                    {}
                                }
                            }
                            if (read)
                            {
                                auto info = read->getInfo().get();
                                otime::RationalTime time =
                                    request->time != time::invalidTime ?
                                    request->time :
                                    info.videoTime.start_time();
                                auto videoData = read->readVideo(time, request->options).get();
                                math::Size2i size;
                                if (!info.video.empty())
                                {
                                    size.h = request->height;
                                    size.w = size.h * info.video[0].size.getAspect();
                                }
                                gl::OffscreenBufferOptions options;
                                options.colorType = image::PixelType::RGBA_U8;
                                if (gl::doCreate(buffer, size, options))
                                {
                                    buffer = gl::OffscreenBuffer::create(size, options);
                                }
                                if (render && buffer && videoData.image)
                                {
                                    try
                                    {
                                        gl::OffscreenBufferBinding binding(buffer);
                                        render->begin(size);
                                        render->drawImage(
                                            videoData.image,
                                            { math::Box2i(0, 0, size.w, size.h) });
                                        render->end();
                                        image = image::Image::create(
                                            size.w,
                                            size.h,
                                            image::PixelType::RGBA_U8);
                                        glPixelStorei(GL_PACK_ALIGNMENT, 1);
                                        glReadPixels(
                                            0,
                                            0,
                                            size.w,
                                            size.h,
                                            GL_RGBA,
                                            GL_UNSIGNED_BYTE,
                                            image->getData());
                                    }
                                    catch (const std::exception&)
                                    {}
                                }
                            }
                        }
                        catch (const std::exception&)
                        {}
                        p.cache->addThumbnail(key, image);
                    }
                    request->promise.set_value(image);
                }
                
                // Handle waveform requests.
                for (const auto& request : waveformRequests)
                {
                    std::shared_ptr<geom::TriangleMesh2> mesh;
                    const std::string key = ThumbnailCache::getWaveformKey(
                        request->size,
                        request->path,
                        request->timeRange,
                        request->options);
                    if (!p.cache->getWaveform(key, mesh))
                    {
                        try
                        {
                            const std::string& fileName = request->path.get();
                            std::shared_ptr<io::IRead> read;
                            if (!p.thread.ioCache.get(fileName, read))
                            {
                                if (auto context = p.context.lock())
                                {
                                    auto ioSystem = context->getSystem<io::System>();
                                    try
                                    {
                                        read = ioSystem->read(
                                            request->path,
                                            request->memoryRead,
                                            request->options);
                                        p.thread.ioCache.add(fileName, read);
                                    }
                                    catch (const std::exception&)
                                    {}
                                }
                            }
                            if (read)
                            {
                                auto info = read->getInfo().get();
                                otime::TimeRange timeRange =
                                    request->timeRange != time::invalidTimeRange ?
                                    request->timeRange :
                                    otime::TimeRange(
                                        otime::RationalTime(0.0, 1.0),
                                        otime::RationalTime(1.0, 1.0));
                                auto audioData = read->readAudio(timeRange, request->options).get();
                                if (audioData.audio)
                                {
                                    auto resample = audio::AudioResample::create(
                                        audioData.audio->getInfo(),
                                        audio::Info(1, audio::DataType::F32, audioData.audio->getSampleRate()));
                                    const auto resampledAudio = resample->process(audioData.audio);
                                    mesh = audioMesh(resampledAudio, request->size);
                                }
                            }
                        }
                        catch (const std::exception&)
                        {}
                        p.cache->addWaveform(key, mesh);
                    }
                    request->promise.set_value(mesh);
                }

                // Logging.
                {
                    const auto now = std::chrono::steady_clock::now();
                    const std::chrono::duration<float> diff = now - p.thread.logTimer;
                    if (diff.count() > 10.F)
                    {
                        p.thread.logTimer = now;
                        size_t infoRequests = 0;
                        size_t thumbnailRequests = 0;
                        size_t waveformRequests = 0;
                        {
                            std::unique_lock<std::mutex> lock(p.mutex.mutex);
                            infoRequests = p.mutex.infoRequests.size();
                            thumbnailRequests = p.mutex.thumbnailRequests.size();
                            waveformRequests = p.mutex.waveformRequests.size();
                        }
                        if (auto context = p.context.lock())
                        {
                            context->log(
                                "tl::ui::ThumbnailGenerator",
                                string::Format(
                                    "\n"
                                    "    Info requests: {0}\n"
                                    "    Thumbnail requests: {1}\n"
                                    "    Waveform requests: {2}\n"
                                    "    Cache: {3}, {4}%\n"
                                    "    I/O cache: {5}, {6}%").
                                    arg(infoRequests).
                                    arg(thumbnailRequests).
                                    arg(waveformRequests).
                                    arg(p.cache->getSize()).
                                    arg(p.cache->getPercentage()).
                                    arg(p.thread.ioCache.getSize()).
                                    arg(p.thread.ioCache.getPercentage()));
                        }
                    }
                }
            }
        }
        
        void ThumbnailGenerator::_cancelRequests()
        {
            TLRENDER_P();
            std::list<std::shared_ptr<Private::InfoRequest> > infoRequests;
            std::list<std::shared_ptr<Private::ThumbnailRequest> > thumbnailRequests;
            std::list<std::shared_ptr<Private::WaveformRequest> > waveformRequests;
            {
                std::unique_lock<std::mutex> lock(p.mutex.mutex);
                infoRequests = std::move(p.mutex.infoRequests);
                thumbnailRequests = std::move(p.mutex.thumbnailRequests);
                waveformRequests = std::move(p.mutex.waveformRequests);
            }
            for (auto& request : infoRequests)
            {
                request->promise.set_value(io::Info());
            }
            for (auto& request : thumbnailRequests)
            {
                request->promise.set_value(nullptr);
            }
            for (auto& request : waveformRequests)
            {
                request->promise.set_value(nullptr);
            }
        }

        struct ThumbnailSystem::Private
        {
            std::shared_ptr<ThumbnailCache> cache;
        };

        void ThumbnailSystem::_init(const std::shared_ptr<system::Context>& context)
        {
            ISystem::_init("tl::ui::ThumbnailSystem", context);
            TLRENDER_P();
            p.cache = ThumbnailCache::create(context);
        }

        ThumbnailSystem::ThumbnailSystem() :
            _p(new Private)
        {}

        ThumbnailSystem::~ThumbnailSystem()
        {}

        std::shared_ptr<ThumbnailSystem> ThumbnailSystem::create(
            const std::shared_ptr<system::Context>& context)
        {
            auto out = std::shared_ptr<ThumbnailSystem>(new ThumbnailSystem);
            out->_init(context);
            return out;
        }
        
        const std::shared_ptr<ThumbnailCache>& ThumbnailSystem::getCache() const
        {
            return _p->cache;
        }
    }
}
