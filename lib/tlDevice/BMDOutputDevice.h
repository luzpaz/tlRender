// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2021-2022 Darby Johnston
// All rights reserved.

#pragma once

#include <tlDevice/IOutputDevice.h>

#include "platform.h"

#include <atomic>
#include <functional>
#include <list>
#include <mutex>

namespace tl
{
    namespace device
    {
        //! Decklink wrapper.
        class DLWrapper
        {
        public:
            ~DLWrapper();

            IDeckLink* p = nullptr;
        };

        //! Decklink configuration wrapper.
        class DLConfigWrapper
        {
        public:
            ~DLConfigWrapper();

            IDeckLinkConfiguration* p = nullptr;
        };

        //! Decklink output wrapper.
        class DLOutputWrapper
        {
        public:
            ~DLOutputWrapper();

            IDeckLinkOutput* p = nullptr;
        };

        //! Decklink output callback.
        class DLOutputCallback :
            public IDeckLinkVideoOutputCallback,
            public IDeckLinkAudioOutputCallback
        {
        public:
            DLOutputCallback(
                IDeckLinkOutput*,
                const imaging::Size& size,
                PixelType pixelType,
                const otime::RationalTime& frameRate);

            void setPlayback(timeline::Playback);
            void pixelData(const std::shared_ptr<device::PixelData>&);
            void audioData(const std::vector<timeline::AudioData>&);

            HRESULT STDMETHODCALLTYPE ScheduledFrameCompleted(IDeckLinkVideoFrame*, BMDOutputFrameCompletionResult) override;
            HRESULT STDMETHODCALLTYPE ScheduledPlaybackHasStopped() override;

            HRESULT STDMETHODCALLTYPE RenderAudioSamples(BOOL preroll) override;

            HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, LPVOID* ppv) override;
            ULONG STDMETHODCALLTYPE AddRef() override;
            ULONG STDMETHODCALLTYPE Release() override;

        private:
            IDeckLinkOutput* _dlOutput = nullptr;
            imaging::Size _size;
            PixelType _pixelType = PixelType::None;
            otime::RationalTime _frameRate = time::invalidTime;

            std::atomic<size_t> _refCount;

            struct PixelDataMutexData
            {
                std::list<std::shared_ptr<device::PixelData> > pixelData;
            };
            PixelDataMutexData _pixelDataMutexData;
            std::mutex _pixelDataMutex;

            struct PixelDataThreadData
            {
                std::shared_ptr<device::PixelData> pixelDataTmp;
                uint64_t frameCount = 0;
            };
            PixelDataThreadData _pixelDataThreadData;

            struct AudioMutexData
            {
                timeline::Playback playback = timeline::Playback::Stop;
                otime::RationalTime currentTime = time::invalidTime;
                std::vector<timeline::AudioData> audioData;
            };
            AudioMutexData _audioMutexData;
            std::mutex _audioMutex;

            struct AudioThreadData
            {
                timeline::Playback playback = timeline::Playback::Stop;
                otime::RationalTime currentTime = time::invalidTime;
                otime::RationalTime startTime = time::invalidTime;
                size_t offset = 0;
            };
            AudioThreadData _audioThreadData;
        };

        //! Decklink output callback wrapper.
        class DLOutputCallbackWrapper
        {
        public:
            ~DLOutputCallbackWrapper();

            DLOutputCallback* p = nullptr;
        };

        //! BMD output device.
        class BMDOutputDevice : public IOutputDevice
        {
            TLRENDER_NON_COPYABLE(BMDOutputDevice);

        protected:
            void _init(
                int deviceIndex,
                int displayModeIndex,
                PixelType,
                const std::shared_ptr<system::Context>&);

            BMDOutputDevice();

        public:
            ~BMDOutputDevice() override;

            //! Create a new BMD output device.
            static std::shared_ptr<BMDOutputDevice> create(
                int deviceIndex,
                int displayModeIndex,
                PixelType,
                const std::shared_ptr<system::Context>&);

            void setPlayback(timeline::Playback) override;
            void pixelData(const std::shared_ptr<device::PixelData>&) override;
            void audioData(const std::vector<timeline::AudioData>&) override;

        private:
            DLWrapper _dl;
            DLConfigWrapper _dlConfig;
            DLOutputWrapper _dlOutput;
            DLOutputCallbackWrapper _dlOutputCallback;
        };
    }
}
