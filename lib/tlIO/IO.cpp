// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2021-2023 Darby Johnston
// All rights reserved.

#include <tlIO/IO.h>

namespace tl
{
    namespace io
    {
        Options merge(const Options& a, const Options& b)
        {
            Options out = a;
            out.insert(b.begin(), b.end());
            return out;
        }
    }
}
