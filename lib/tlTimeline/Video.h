// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2021-2023 Darby Johnston
// All rights reserved.

#pragma once

#include <tlTimeline/ImageOptions.h>
#include <tlTimeline/Transition.h>

#include <tlCore/Image.h>
#include <tlCore/Time.h>

namespace tl
{
    namespace timeline
    {
        //! Video layer.
        struct VideoLayer
        {
            std::shared_ptr<image::Image> image;
            ImageOptions imageOptions;

            std::shared_ptr<image::Image> imageB;
            ImageOptions imageOptionsB;

            Transition transition = Transition::None;
            float transitionValue = 0.F;

            bool operator == (const VideoLayer&) const;
            bool operator != (const VideoLayer&) const;
        };

        //! Video data.
        struct VideoData
        {
            otime::RationalTime time = time::invalidTime;
            std::vector<VideoLayer> layers;

            bool operator == (const VideoData&) const;
            bool operator != (const VideoData&) const;
        };

        //! Compare the time values of video data.
        bool isTimeEqual(const VideoData&, const VideoData&);
    }
}

#include <tlTimeline/VideoInline.h>
