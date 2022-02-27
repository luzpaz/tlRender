// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2021-2022 Darby Johnston
// All rights reserved.

#include <tlCoreTest/RangeTest.h>

#include <tlCore/Assert.h>
#include <tlCore/Range.h>

using namespace tl::core;
using namespace tl::core::math;

namespace tl
{
    namespace tests
    {
        namespace core_test
        {
            RangeTest::RangeTest(const std::shared_ptr<system::Context>& context) :
                ITest("core_test::RangeTest", context)
            {}

            std::shared_ptr<RangeTest> RangeTest::create(const std::shared_ptr<system::Context>& context)
            {
                return std::shared_ptr<RangeTest>(new RangeTest(context));
            }

            void RangeTest::run()
            {
                {
                    const auto r = IntRange();
                    TLRENDER_ASSERT(0 == r.getMin());
                    TLRENDER_ASSERT(0 == r.getMax());
                }
                {
                    const auto r = IntRange(1);
                    TLRENDER_ASSERT(1 == r.getMin());
                    TLRENDER_ASSERT(1 == r.getMax());
                }
                {
                    const auto r = IntRange(1, 10);
                    TLRENDER_ASSERT(1 == r.getMin());
                    TLRENDER_ASSERT(10 == r.getMax());
                }
                {
                    auto r = IntRange(1, 10);
                    r.zero();
                    TLRENDER_ASSERT(0 == r.getMin());
                    TLRENDER_ASSERT(0 == r.getMax());
                }
                {
                    const auto r = IntRange(1, 10);
                    TLRENDER_ASSERT(r.contains(1));
                    TLRENDER_ASSERT(r.contains(10));
                    TLRENDER_ASSERT(!r.contains(0));
                    TLRENDER_ASSERT(!r.contains(11));
                }
                {
                    const auto r = IntRange(1, 10);
                    TLRENDER_ASSERT(r.intersects(IntRange(0, 1)));
                    TLRENDER_ASSERT(r.intersects(IntRange(10, 11)));
                    TLRENDER_ASSERT(!r.intersects(IntRange(12, 20)));
                }
                {
                    auto r = IntRange(1, 10);
                    r.expand(20);
                    TLRENDER_ASSERT(IntRange(1, 20) == r);
                }
                {
                    auto r = IntRange(1, 10);
                    r.expand(IntRange(0, 20));
                    TLRENDER_ASSERT(IntRange(0, 20) == r);
                }
                {
                    TLRENDER_ASSERT(IntRange(1, 10) == IntRange(1, 10));
                    TLRENDER_ASSERT(IntRange(1, 10) != IntRange(0, 11));
                    TLRENDER_ASSERT(IntRange(0, 10) < IntRange(1, 11));
                }
            }
        }
    }
}
