// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2021 Darby Johnston
// All rights reserved.

#include <tlrCore/Cineon.h>

#include <tlrCore/Assert.h>
#include <tlrCore/Error.h>
#include <tlrCore/Memory.h>
#include <tlrCore/StringFormat.h>

#include <array>
#include <cstring>
#include <sstream>

namespace tlr
{
    namespace cineon
    {
        TLR_ENUM_IMPL(
            Orient,
            "LeftRightTopBottom",
            "LeftRightBottomTop",
            "RightLeftTopBottom",
            "RightLeftBottomTop",
            "TopBottomLeftRight",
            "TopBottomRightLeft",
            "BottomTopLeftRight",
            "BottomTopRightLeft" );

        TLR_ENUM_IMPL(
            Descriptor,
            "Luminance",
            "RedFilmPrint",
            "GreenFilmPrint",
            "BlueFilmPrint",
            "RedCCIRXA11",
            "GreenCCIRXA11",
            "BlueCCIRXA11");

        namespace
        {
            void zero(int32_t* value)
            {
                *((uint32_t*)value) = 0x80000000;
            }

            void zero(float* value)
            {
                *((uint32_t*)value) = 0x7F800000;
            }

            void zero(char* value, size_t size)
            {
                memset(value, 0, size);
            }

        } // namespace

        Header::Header()
        {
            memset(&file, 0xff, sizeof(Header::File));
            zero(file.version, 8);
            zero(file.name, 100);
            zero(file.time, 24);

            memset(&image, 0xff, sizeof(Header::Image));

            for (uint8_t i = 0; i < 8; ++i)
            {
                zero(&image.channel[i].lowData);
                zero(&image.channel[i].lowQuantity);
                zero(&image.channel[i].highData);
                zero(&image.channel[i].highQuantity);
            }

            memset(&source, 0xff, sizeof(Header::Source));
            zero(&source.offset[0]);
            zero(&source.offset[1]);
            zero(source.file, 100);
            zero(source.time, 24);
            zero(source.inputDevice, 64);
            zero(source.inputModel, 32);
            zero(source.inputSerial, 32);
            zero(&source.inputPitch[0]);
            zero(&source.inputPitch[1]);
            zero(&source.gamma);

            memset(&film, 0xff, sizeof(Header::Film));
            zero(film.format, 32);
            zero(&film.frameRate);
            zero(film.frameId, 32);
            zero(film.slate, 200);
        }

        void Header::convertEndian()
        {
            memory::endian(&file.imageOffset, 1, 4);
            memory::endian(&file.headerSize, 1, 4);
            memory::endian(&file.industryHeaderSize, 1, 4);
            memory::endian(&file.userHeaderSize, 1, 4);
            memory::endian(&file.size, 1, 4);

            for (uint8_t i = 0; i < 8; ++i)
            {
                memory::endian(image.channel[i].size, 2, 4);
                memory::endian(&image.channel[i].lowData, 1, 4);
                memory::endian(&image.channel[i].lowQuantity, 1, 4);
                memory::endian(&image.channel[i].highData, 1, 4);
                memory::endian(&image.channel[i].highQuantity, 1, 4);
            }

            memory::endian(image.white, 2, 4);
            memory::endian(image.red, 2, 4);
            memory::endian(image.green, 2, 4);
            memory::endian(image.blue, 2, 4);
            memory::endian(&image.linePadding, 1, 4);
            memory::endian(&image.channelPadding, 1, 4);

            memory::endian(source.offset, 2, 4);
            memory::endian(source.inputPitch, 2, 4);
            memory::endian(&source.gamma, 1, 4);

            memory::endian(&film.prefix, 1, 4);
            memory::endian(&film.count, 1, 4);
            memory::endian(&film.frame, 1, 4);
            memory::endian(&film.frameRate, 1, 4);
        }

        namespace
        {
            bool isValid(const uint8_t* in)
            {
                return *in != 0xff;
            }

            // Constants to catch uninitialized values.
            const int32_t _intMax = 1000000;
            const float   _floatMax = 1000000.F;
            const float   _minSpeed = .000001F;

            bool isValid(const uint32_t* in)
            {
                return
                    *in != 0xffffffff &&
                    *in < static_cast<uint32_t>(_intMax);
            }

            bool isValid(const int32_t* in)
            {
                return
                    *in != static_cast<int32_t>(0x80000000) &&
                    *in > -_intMax &&
                    *in < _intMax;
            }

            bool isValid(const float* in)
            {
                return
                    *(reinterpret_cast<const uint32_t*>(in)) != 0x7F800000 &&
                    *in > -_floatMax &&
                    *in < _floatMax;
            }

        } // namespace

        Header Header::read(const std::shared_ptr<file::FileIO>& io, avio::Info& info)
        {
            Header out;

            // Read the file section of the header.
            io->read(&out.file, sizeof(Header::File));

            // Check the magic number.
            bool convertEndian = false;
            if (magic[0] == out.file.magic)
                ;
            else if (magic[1] == out.file.magic)
            {
                convertEndian = true;
            }
            else
            {
                throw std::runtime_error(string::Format("{0}: {1}").
                    arg(io->getFileName()).
                    arg("Bad magic number"));
            }

            // Read the rest of the header.
            io->read(&out.image, sizeof(Header::Image));
            io->read(&out.source, sizeof(Header::Source));
            io->read(&out.film, sizeof(Header::Film));

            // Convert the endian if necessary.
            imaging::Info imageInfo;
            if (convertEndian)
            {
                io->setEndianConversion(true);
                out.convertEndian();
                imageInfo.layout.endian = memory::opposite(memory::getEndian());
            }

            // Read the image section of the header.
            if (!out.image.channels)
            {
                throw std::runtime_error(string::Format("{0}: {1}").
                    arg(io->getFileName()).
                    arg("No image channels"));
            }
            uint8_t i = 1;
            for (; i < out.image.channels; ++i)
            {
                if ((out.image.channel[i].size[0] != out.image.channel[0].size[0]) ||
                    (out.image.channel[i].size[1] != out.image.channel[0].size[1]))
                {
                    break;
                }
                if (out.image.channel[i].bitDepth != out.image.channel[0].bitDepth)
                {
                    break;
                }
            }
            if (i < out.image.channels)
            {
                throw std::runtime_error(string::Format("{0}: {1}").
                    arg(io->getFileName()).
                    arg("Unsupported image channels"));
            }
            imaging::PixelType pixelType = imaging::PixelType::None;
            switch (out.image.channels)
            {
            case 3:
                switch (out.image.channel[0].bitDepth)
                {
                case 10:
                    pixelType = imaging::PixelType::RGB_U10;
                    break;
                default: break;
                }
                break;
            default: break;
            }
            if (imaging::PixelType::None == pixelType)
            {
                throw std::runtime_error(string::Format("{0}: {1}").
                    arg(io->getFileName()).
                    arg("Unsupported bit depth"));
            }
            if (isValid(&out.image.linePadding) && out.image.linePadding)
            {
                throw std::runtime_error(string::Format("{0}: {1}").
                    arg(io->getFileName()).
                    arg("Unsupported line padding"));
            }
            if (isValid(&out.image.channelPadding) && out.image.channelPadding)
            {
                throw std::runtime_error(string::Format("{0}: {1}").
                    arg(io->getFileName()).
                    arg("Unsupported channel padding"));
            }

            // Collect information.
            imageInfo.pixelType = pixelType;
            imageInfo.size.w = out.image.channel[0].size[0];
            imageInfo.size.h = out.image.channel[0].size[1];
            if (io->getSize() - out.file.imageOffset != imaging::getDataByteCount(imageInfo))
            {
                throw std::runtime_error(string::Format("{0}: {1}").
                    arg(io->getFileName()).
                    arg("Incomplete file"));
            }
            switch (static_cast<Orient>(out.image.orient))
            {
            case Orient::LeftRightBottomTop:
                imageInfo.layout.mirror.y = true;
                break;
            case Orient::RightLeftTopBottom:
                imageInfo.layout.mirror.x = true;
                break;
            case Orient::RightLeftBottomTop:
                imageInfo.layout.mirror.x = true;
                imageInfo.layout.mirror.y = true;
                break;
            case Orient::TopBottomLeftRight:
            case Orient::TopBottomRightLeft:
            case Orient::BottomTopLeftRight:
            case Orient::BottomTopRightLeft:
                //! \todo Implement these image orientations.
                break;
            default: break;
            }
            info.video.push_back(imageInfo);
            if (isValid(out.file.time, 24))
            {
                info.tags["Time"] = toString(out.file.time, 24);
            }
            if (isValid(&out.source.offset[0]) && isValid(&out.source.offset[1]))
            {
                std::stringstream ss;
                ss << out.source.offset[0] << " " << out.source.offset[1];
                info.tags["Source Offset"] = ss.str();
            }
            if (isValid(out.source.file, 100))
            {
                info.tags["Source File"] = toString(out.source.file, 100);
            }
            if (isValid(out.source.time, 24))
            {
                info.tags["Source Time"] = toString(out.source.time, 24);
            }
            if (isValid(out.source.inputDevice, 64))
            {
                info.tags["Source Input Device"] = toString(out.source.inputDevice, 64);
            }
            if (isValid(out.source.inputModel, 32))
            {
                info.tags["Source Input Model"] = toString(out.source.inputModel, 32);
            }
            if (isValid(out.source.inputSerial, 32))
            {
                info.tags["Source Input Serial"] = toString(out.source.inputSerial, 32);
            }
            if (isValid(&out.source.inputPitch[0]) && isValid(&out.source.inputPitch[1]))
            {
                std::stringstream ss;
                ss << out.source.inputPitch[0] << " " << out.source.inputPitch[1];
                info.tags["Source Input Pitch"] = ss.str();
            }
            if (isValid(&out.source.gamma))
            {
                std::stringstream ss;
                ss << out.source.gamma;
                info.tags["Source Gamma"] = ss.str();
            }
            if (isValid(&out.film.id) &&
                isValid(&out.film.type) &&
                isValid(&out.film.offset) &&
                isValid(&out.film.prefix) &&
                isValid(&out.film.count))
            {
                info.tags["Keycode"] = time::keycodeToString(
                    out.film.id,
                    out.film.type,
                    out.film.prefix,
                    out.film.count,
                    out.film.offset);
            }
            if (isValid(out.film.format, 32))
            {
                info.tags["Film Format"] = toString(out.film.format, 32);
            }
            if (isValid(&out.film.frame))
            {
                std::stringstream ss;
                ss << out.film.frame;
                info.tags["Film Frame"] = ss.str();
            }
            if (isValid(&out.film.frameRate) && out.film.frameRate >= _minSpeed)
            {
                info.videoDuration = otime::RationalTime(1.0, out.film.frameRate);
                std::stringstream ss;
                ss << out.film.frameRate;
                info.tags["Film Frame Rate"] = ss.str();
            }
            if (isValid(out.film.frameId, 32))
            {
                info.tags["Film Frame ID"] = toString(out.film.frameId, 32);
            }
            if (isValid(out.film.slate, 200))
            {
                info.tags["Film Slate"] = toString(out.film.slate, 200);
            }

            // Set the file position.
            if (out.file.imageOffset)
            {
                io->setPos(out.file.imageOffset);
            }

            return out;
        }

        void Header::write(const std::shared_ptr<file::FileIO>& io, const avio::Info& info)
        {
            Header header;

            // Set the file section.
            header.file.imageOffset = 2048;
            header.file.headerSize = 1024;
            header.file.industryHeaderSize = 1024;
            header.file.userHeaderSize = 0;

            // Set the image section.
            header.image.orient = static_cast<uint8_t>(Orient::LeftRightTopBottom);
            header.image.channels = 3;
            header.image.channel[0].descriptor[1] = static_cast<uint8_t>(Descriptor::RedFilmPrint);
            header.image.channel[1].descriptor[1] = static_cast<uint8_t>(Descriptor::GreenFilmPrint);
            header.image.channel[2].descriptor[1] = static_cast<uint8_t>(Descriptor::BlueFilmPrint);
            const uint8_t bitDepth = 10;
            for (uint8_t i = 0; i < header.image.channels; ++i)
            {
                header.image.channel[i].descriptor[0] = 0;
                header.image.channel[i].bitDepth = bitDepth;
                header.image.channel[i].size[0] = info.video[0].size.w;
                header.image.channel[i].size[1] = info.video[0].size.h;

                header.image.channel[i].lowData = 0;

                switch (bitDepth)
                {
                case  8: header.image.channel[i].highData = imaging::U8Range.getMax();  break;
                case 10: header.image.channel[i].highData = imaging::U10Range.getMax(); break;
                case 12: header.image.channel[i].highData = imaging::U12Range.getMax(); break;
                case 16: header.image.channel[i].highData = imaging::U16Range.getMax(); break;
                default: break;
                }
            }
            header.image.interleave = 0;
            header.image.packing = 5;
            header.image.dataSign = 0;
            header.image.dataSense = 0;
            header.image.linePadding = 0;
            header.image.channelPadding = 0;

            // Set the tags.
            auto i = info.tags.find("Time");
            if (i != info.tags.end())
            {
                fromString(i->second, header.file.time, 24, false);
            }
            i = info.tags.find("Source Offset");
            if (i != info.tags.end())
            {
                std::stringstream ss(i->second);
                ss >> header.source.offset[0];
                ss >> header.source.offset[1];
            }
            i = info.tags.find("Source File");
            if (i != info.tags.end())
            {
                fromString(i->second, header.source.file, 100, false);
            }
            i = info.tags.find("Source Time");
            if (i != info.tags.end())
            {
                fromString(i->second, header.source.time, 24, false);
            }
            i = info.tags.find("Source Input Device");
            if (i != info.tags.end())
            {
                fromString(i->second, header.source.inputDevice, 64, false);
            }
            i = info.tags.find("Source Input Model");
            if (i != info.tags.end())
            {
                fromString(i->second, header.source.inputModel, 32, false);
            }
            i = info.tags.find("Source Input Serial");
            if (i != info.tags.end())
            {
                fromString(i->second, header.source.inputSerial, 32, false);
            }
            i = info.tags.find("Source Input Pitch");
            if (i != info.tags.end())
            {
                std::stringstream ss(i->second);
                ss >> header.source.inputPitch[0];
                ss >> header.source.inputPitch[1];
            }
            i = info.tags.find("Source Gamma");
            if (i != info.tags.end())
            {
                std::stringstream ss(i->second);
                ss >> header.source.gamma;
            }
            i = info.tags.find("Keycode");
            if (i != info.tags.end())
            {
                int id = 0;
                int type = 0;
                int prefix = 0;
                int count = 0;
                int offset = 0;
                time::stringToKeycode(i->second, id, type, prefix, count, offset);
                header.film.id = id;
                header.film.type = type;
                header.film.offset = offset;
                header.film.prefix = prefix;
                header.film.count = count;
            }
            i = info.tags.find("Film Format");
            if (i != info.tags.end())
            {
                fromString(i->second, header.film.format, 32, false);
            }
            i = info.tags.find("Film Frame");
            if (i != info.tags.end())
            {
                std::stringstream ss(i->second);
                ss >> header.film.frame;
            }
            i = info.tags.find("Film Frame Rate");
            if (i != info.tags.end())
            {
                std::stringstream ss(i->second);
                ss >> header.film.frameRate;
            }
            i = info.tags.find("Film Frame ID");
            if (i != info.tags.end())
            {
                fromString(i->second, header.film.frameId, 32, false);
            }
            i = info.tags.find("Film Slate");
            if (i != info.tags.end())
            {
                fromString(i->second, header.film.slate, 200, false);
            }

            // Write the header.
            const bool convertEndian = memory::getEndian() != memory::Endian::MSB;
            io->setEndianConversion(convertEndian);
            if (convertEndian)
            {
                header.convertEndian();
                header.file.magic = magic[1];
            }
            else
            {
                header.file.magic = magic[0];
            }
            io->write(&header.file, sizeof(Header::File));
            io->write(&header.image, sizeof(Header::Image));
            io->write(&header.source, sizeof(Header::Source));
            io->write(&header.film, sizeof(Header::Film));
        }

        void Header::finishWrite(const std::shared_ptr<file::FileIO>& io)
        {
            const uint32_t size = static_cast<uint32_t>(io->getPos());
            io->setPos(20);
            io->writeU32(size);
        }

        bool isValid(const char* in, size_t size)
        {
            const char _minChar = 32;
            const char _maxChar = 126;
            const char* p = in;
            const char* const end = p + size;
            for (; *p && p < end; ++p)
            {
                if (*p < _minChar || *p > _maxChar)
                {
                    return false;
                }
            }
            return size ? (in[0] != 0) : false;
        }

        std::string toString(const char* in, size_t size)
        {
            const char* p = in;
            const char* const end = p + size;
            for (; *p && p < end; ++p)
                ;
            return std::string(in, p - in);
        }

        size_t fromString(
            const std::string& string,
            char* out,
            size_t             maxLen,
            bool               terminate)
        {
            TLR_ASSERT(maxLen >= 0);
            const char* c = string.c_str();
            const size_t length = std::min(string.length(), maxLen - static_cast<int>(terminate));
            size_t i = 0;
            for (; i < length; ++i)
            {
                out[i] = c[i];
            }
            if (terminate)
            {
                out[i++] = 0;
            }
            return i;
        }

        void Plugin::_init()
        {
            IPlugin::_init(
                "Cineon",
                { ".cin" });
        }

        Plugin::Plugin()
        {}
            
        std::shared_ptr<Plugin> Plugin::create()
        {
            auto out = std::shared_ptr<Plugin>(new Plugin);
            out->_init();
            return out;
        }

        std::shared_ptr<avio::IRead> Plugin::read(
            const std::string& fileName,
            const avio::Options& options)
        {
            return Read::create(fileName, options);
        }

        std::vector<imaging::PixelType> Plugin::getWritePixelTypes() const
        {
            return
            {
                imaging::PixelType::RGB_U10
            };
        }

        uint8_t Plugin::getWriteAlignment() const
        {
            return 4;
        }

        memory::Endian Plugin::getWriteEndian() const
        {
            return memory::Endian::MSB;
        }

        std::shared_ptr<avio::IWrite> Plugin::write(
            const std::string& fileName,
            const avio::Info& info,
            const avio::Options& options)
        {
            return !info.video.empty() && _isWriteCompatible(info.video[0]) ?
                Write::create(fileName, info, options) :
                nullptr;
        }
    }

    TLR_ENUM_SERIALIZE_IMPL(cineon, Orient);
    TLR_ENUM_SERIALIZE_IMPL(cineon, Descriptor);
}
