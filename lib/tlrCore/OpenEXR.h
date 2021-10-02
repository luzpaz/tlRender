// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2021 Darby Johnston
// All rights reserved.

#pragma once

#include <tlrCore/BBox.h>
#include <tlrCore/SequenceIO.h>

#include <ImathBox.h>
#include <ImfHeader.h>
#include <ImfInputFile.h>
#include <ImfPixelType.h>

namespace tlr
{
    //! OpenEXR I/O.
    namespace exr
    {
        //! Channel grouping.
        enum class ChannelGrouping
        {
            None,
            Known,
            All,

            Count,
            First = None
        };
        TLR_ENUM(ChannelGrouping);
        TLR_ENUM_SERIALIZE(ChannelGrouping);

        //! Image channel.
        struct Channel
        {
            Channel();
            Channel(
                const std::string&    name,
                Imf::PixelType        pixelType,
                const math::Vector2i& sampling  = math::Vector2i(1, 1));

            std::string    name;
            Imf::PixelType pixelType = Imf::PixelType::HALF;
            math::Vector2i sampling  = math::Vector2i(1, 1);
        };

        //! Image layer.
        struct Layer
        {
            Layer(
                const std::vector<Channel>& channels        = std::vector<Channel>(),
                bool                        luminanceChroma = false);

            std::string          name;
            std::vector<Channel> channels;
            bool                 luminanceChroma = false;
        };

        //! Compression types.
        enum class Compression
        {
            None,
            RLE,
            ZIPS,
            ZIP,
            PIZ,
            PXR24,
            B44,
            B44A,
            DWAA,
            DWAB,

            Count,
            First = None
        };
        TLR_ENUM(Compression);
        TLR_ENUM_SERIALIZE(Compression);

        //! Get a layer name from a list of channel names.
        std::string getLayerName(const std::vector<std::string>&);

        //! Get the channels that aren't in any layer.
        Imf::ChannelList getDefaultLayer(const Imf::ChannelList&);

        //! Find a channel by name.
        const Imf::Channel* find(const Imf::ChannelList&, std::string&);

        //! Get a list of layers from Imf channels.
        std::vector<Layer> getLayers(const Imf::ChannelList&, ChannelGrouping);

        //! Read the tags from an Imf header.
        void readTags(const Imf::Header&, std::map<std::string, std::string>&);

        //! Write tags to an Imf header.
        //!
        //! \todo Write all the tags that are handled by readTags().
        void writeTags(const std::map<std::string, std::string>&, double speed, Imf::Header&);

        //! Convert an Imath box type.
        math::BBox2i fromImath(const Imath::Box2i&);

        //! Convert from an Imf channel.
        Channel fromImf(const std::string& name, const Imf::Channel&);

        //! Memory-mapped input stream.
        class MemoryMappedIStream : public Imf::IStream
        {
            TLR_NON_COPYABLE(MemoryMappedIStream);

        public:
            MemoryMappedIStream(const char fileName[]);
            ~MemoryMappedIStream() override;

            bool isMemoryMapped() const override;
            char* readMemoryMapped(int n) override;
            bool read(char c[], int n) override;
            uint64_t tellg() override;
            void seekg(uint64_t pos) override;

        private:
            TLR_PRIVATE();
        };

        //! OpenEXR reader.
        class Read : public avio::ISequenceRead
        {
        protected:
            void _init(
                const file::Path&,
                const avio::Options&,
                const std::shared_ptr<core::LogSystem>&);
            Read();

        public:
            ~Read() override;

            //! Create a new reader.
            static std::shared_ptr<Read> create(
                const file::Path&,
                const avio::Options&,
                const std::shared_ptr<core::LogSystem>&);

        protected:
            avio::Info _getInfo(const std::string& fileName) override;
            avio::VideoFrame _readVideoFrame(
                const std::string& fileName,
                const otime::RationalTime&,
                uint16_t layer,
                const std::shared_ptr<imaging::Image>&) override;

        private:
            ChannelGrouping _channelGrouping = ChannelGrouping::Known;
        };

        //! OpenEXR writer.
        class Write : public avio::ISequenceWrite
        {
        protected:
            void _init(
                const file::Path&,
                const avio::Info&,
                const avio::Options&,
                const std::shared_ptr<core::LogSystem>&);
            Write();

        public:
            ~Write() override;

            //! Create a new writer.
            static std::shared_ptr<Write> create(
                const file::Path&,
                const avio::Info&,
                const avio::Options&,
                const std::shared_ptr<core::LogSystem>&);

        protected:
            void _writeVideoFrame(
                const std::string& fileName,
                const otime::RationalTime&,
                const std::shared_ptr<imaging::Image>&) override;
        };

        //! OpenEXR plugin.
        class Plugin : public avio::IPlugin
        {
        protected:
            void _init(const std::shared_ptr<core::LogSystem>&);
            Plugin();

        public:
            //! Create a new plugin.
            static std::shared_ptr<Plugin> create(const std::shared_ptr<core::LogSystem>&);

            std::shared_ptr<avio::IRead> read(
                const file::Path&,
                const avio::Options& = avio::Options()) override;
            std::vector<imaging::PixelType> getWritePixelTypes() const override;
            std::shared_ptr<avio::IWrite> write(
                const file::Path&,
                const avio::Info&,
                const avio::Options& = avio::Options()) override;
        };
    }
}
