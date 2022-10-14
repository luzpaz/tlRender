// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2021-2022 Darby Johnston
// All rights reserved.

#include <tlIOTest/SGITest.h>

#include <tlIO/IOSystem.h>
#include <tlIO/SGI.h>

#include <tlCore/Assert.h>

#include <sstream>

using namespace tl::io;

namespace tl
{
    namespace io_tests
    {
        SGITest::SGITest(const std::shared_ptr<system::Context>& context) :
            ITest("io_test::SGITest", context)
        {}

        std::shared_ptr<SGITest> SGITest::create(const std::shared_ptr<system::Context>& context)
        {
            return std::shared_ptr<SGITest>(new SGITest(context));
        }

        void SGITest::run()
        {
            _io();
        }

        void SGITest::_io()
        {
            auto plugin = _context->getSystem<System>()->getPlugin<sgi::Plugin>();
            for (const auto& fileName : std::vector<std::string>(
                {
                    "SGITest",
                    "大平原"
                }))
            {
                for (const auto& size : std::vector<imaging::Size>(
                    {
                        imaging::Size(16, 16),
                        imaging::Size(1, 1),
                        imaging::Size(0, 0)
                    }))
                {
                    for (const auto& pixelType : imaging::getPixelTypeEnums())
                    {
                        auto imageInfo = plugin->getWriteInfo(imaging::Info(size, pixelType));
                        if (imageInfo.isValid())
                        {
                            file::Path path;
                            {
                                std::stringstream ss;
                                ss << fileName << '_' << size << '_' << pixelType << ".0.sgi";
                                _print(ss.str());
                                path = file::Path(ss.str());
                            }
                            auto image = imaging::Image::create(imageInfo);
                            image->zero();
                            try
                            {
                                {
                                    Info info;
                                    info.video.push_back(imageInfo);
                                    info.videoTime = otime::TimeRange(otime::RationalTime(0.0, 24.0), otime::RationalTime(1.0, 24.0));
                                    auto write = plugin->write(path, info);
                                    _print(path.get());
                                    write->writeVideo(otime::RationalTime(0.0, 24.0), image);
                                }
                                {
                                    auto read = plugin->read(path);
                                    const auto videoData = read->readVideo(otime::RationalTime(0.0, 24.0)).get();
                                    TLRENDER_ASSERT(videoData.image);
                                    TLRENDER_ASSERT(videoData.image->getSize() == image->getSize());
                                    //! \todo Compare image data.
                                    //TLRENDER_ASSERT(0 == memcmp(
                                    //    videoData.image->getData(),
                                    //    image->getData(),
                                    //    image->getDataByteCount()));
                                }
                                {
                                    std::vector<uint8_t> memoryData;
                                    std::vector<file::MemoryRead> memory;
                                    {
                                        auto fileIO = file::FileIO::create(path.get(), file::Mode::Read);
                                        memoryData.resize(fileIO->getSize());
                                        fileIO->read(memoryData.data(), memoryData.size());
                                        memory.push_back(file::MemoryRead(memoryData.data(), memoryData.size()));
                                    }
                                    auto read = plugin->read(path, memory);
                                    const auto videoData = read->readVideo(otime::RationalTime(0.0, 24.0)).get();
                                    TLRENDER_ASSERT(videoData.image);
                                    TLRENDER_ASSERT(videoData.image->getSize() == image->getSize());
                                    //! \todo Compare image data.
                                    //TLRENDER_ASSERT(0 == memcmp(
                                    //    videoData.image->getData(),
                                    //    image->getData(),
                                    //    image->getDataByteCount()));
                                }
                                {
                                    auto io = file::FileIO::create(path.get(), file::Mode::Read);
                                    const size_t size = io->getSize();
                                    io.reset();
                                    file::truncate(path.get(), size / 2);
                                    auto read = plugin->read(path);
                                    const auto videoData = read->readVideo(otime::RationalTime(0.0, 24.0)).get();
                                }
                            }
                            catch (const std::exception& e)
                            {
                                _printError(e.what());
                            }
                        }
                    }
                }
            }
        }
    }
}
