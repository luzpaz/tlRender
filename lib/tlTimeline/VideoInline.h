// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2021-2023 Darby Johnston
// All rights reserved.

namespace tl
{
    namespace timeline
    {
        inline bool VideoLayer::operator == (const VideoLayer& other) const
        {
            return
                image == other.image &&
                imageOptions == other.imageOptions &&
                imageB == other.imageB &&
                imageOptionsB == other.imageOptionsB &&
                transition == other.transition &&
                transitionValue == other.transitionValue;
        }

        inline bool VideoLayer::operator != (const VideoLayer& other) const
        {
            return !(*this == other);
        }

        inline bool VideoData::operator == (const VideoData& other) const
        {
            return
                time::compareExact(time, other.time) &&
                layers == other.layers;
        }

        inline bool VideoData::operator != (const VideoData& other) const
        {
            return !(*this == other);
        }

        inline bool isTimeEqual(const VideoData& a, const VideoData& b)
        {
            return time::compareExact(a.time, b.time);
        }
    }
}
