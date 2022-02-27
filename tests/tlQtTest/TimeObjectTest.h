// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2021-2022 Darby Johnston
// All rights reserved.

#pragma once

#include <tlTestLib/ITest.h>

namespace tl
{
    namespace tests
    {
        namespace qt_test
        {
            class TimeObjectTest : public Test::ITest
            {
            protected:
                TimeObjectTest(const std::shared_ptr<core::system::Context>&);

            public:
                static std::shared_ptr<TimeObjectTest> create(const std::shared_ptr<core::system::Context>&);

                void run() override;
            };
        }
    }
}
