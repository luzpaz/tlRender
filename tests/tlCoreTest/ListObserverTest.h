// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2021-2022 Darby Johnston
// All rights reserved.

#pragma once

#include <tlTestLib/ITest.h>

namespace tl
{
    namespace tests
    {
        namespace core_test
        {
            class ListObserverTest : public Test::ITest
            {
            protected:
                ListObserverTest(const std::shared_ptr<core::system::Context>&);

            public:
                static std::shared_ptr<ListObserverTest> create(const std::shared_ptr<core::system::Context>&);

                void run() override;
            };
        }
    }
}
