// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2021-2022 Darby Johnston
// All rights reserved.

#pragma once

#include <tlGL/Render.h>

#include <tlGL/Mesh.h>
#include <tlGL/OffscreenBuffer.h>
#include <tlGL/Shader.h>
#include <tlGL/Texture.h>

#include <tlCore/ColorConfig.h>
#include <tlCore/LRUCache.h>

#include <OpenColorIO/OpenColorIO.h>

#include <list>

namespace OCIO = OCIO_NAMESPACE;

namespace tl
{
    namespace gl
    {
        std::string colorFunctionName();
        std::string colorFunctionNoOp();
        std::string vertexSource();
        std::string meshFragmentSource();
        std::string textFragmentSource();
        std::string textureFragmentSource();
        std::string imageFragmentSource();
        std::string displayFragmentSource();
        std::string dissolveFragmentSource();
        std::string differenceFragmentSource();

        struct Pos2_F32
        {
            float vx;
            float vy;
        };

        struct Pos2_F32_UV_U16
        {
            float    vx;
            float    vy;
            uint16_t tx;
            uint16_t ty;
        };

        void copyTextures(
            const std::shared_ptr<imaging::Image>&,
            const std::vector<std::shared_ptr<Texture> >&,
            size_t offset = 0);

        class TextureCache
        {
        public:
            void setSize(size_t);

            std::vector<std::shared_ptr<Texture> > get(
                const imaging::Info&,
                const timeline::ImageFilters&,
                size_t offset = 0);

            void add(
                const imaging::Info&,
                const timeline::ImageFilters&,
                const std::vector<std::shared_ptr<Texture> >&);

        private:
            void _cacheUpdate();

            size_t _size = 6;

            struct TextureData
            {
                imaging::Info info;
                timeline::ImageFilters imageFilters;
                std::vector<std::shared_ptr<Texture> > texture;
            };

            std::list<TextureData> _cache;
        };

        struct Render::Private
        {
            imaging::ColorConfig colorConfig;
            OCIO::ConstConfigRcPtr ocioConfig;
            OCIO::DisplayViewTransformRcPtr ocioTransform;
            OCIO::LegacyViewingPipelineRcPtr ocioVP;
            OCIO::ConstProcessorRcPtr ocioProcessor;
            OCIO::ConstGPUProcessorRcPtr ocioGpuProcessor;
            OCIO::GpuShaderDescRcPtr ocioShaderDesc;
            struct TextureId
            {
                TextureId(
                    unsigned    id,
                    std::string name,
                    std::string sampler,
                    unsigned    type);

                unsigned    id = -1;
                std::string name;
                std::string sampler;
                unsigned    type = -1;
            };
            std::vector<TextureId> colorTextures;

            imaging::Size size;

            std::map<std::string, std::shared_ptr<Shader> > shaders;
            std::map<std::string, std::shared_ptr<OffscreenBuffer> > buffers;
            TextureCache textureCache;
            memory::LRUCache<imaging::GlyphInfo, std::shared_ptr<Texture> > glyphTextureCache;
            std::map<std::string, std::shared_ptr<gl::VBO> > vbos;
            std::map<std::string, std::shared_ptr<gl::VAO> > vaos;
        };
    }
}
