// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2021-2024 Darby Johnston
// All rights reserved.

#pragma once

#include <tlTimeline/DisplayOptions.h>
#include <tlTimeline/ImageOptions.h>
#include <tlTimeline/LUTOptions.h>
#include <tlTimeline/OCIOOptions.h>

#include <tlCore/ValueObserver.h>

namespace tl
{
    namespace system
    {
        class Context;
    }

    namespace play
    {
        //! Color model.
        class ColorModel : public std::enable_shared_from_this<ColorModel>
        {
            TLRENDER_NON_COPYABLE(ColorModel);

        protected:
            void _init(const std::shared_ptr<system::Context>&);

            ColorModel();

        public:
            ~ColorModel();

            //! Create a new model.
            static std::shared_ptr<ColorModel> create(const std::shared_ptr<system::Context>&);

            //! Get the OpenColorIO options.
            const timeline::OCIOOptions& getOCIOOptions() const;

            //! Observe the OpenColorIO options.
            std::shared_ptr<observer::IValue<timeline::OCIOOptions> > observeOCIOOptions() const;

            //! Set the OpenColorIO options.
            void setOCIOOptions(const timeline::OCIOOptions&);

            //! Get the LUT options.
            const timeline::LUTOptions& getLUTOptions() const;

            //! Observe the LUT options.
            std::shared_ptr<observer::IValue<timeline::LUTOptions> > observeLUTOptions() const;

            //! Set the LUT options.
            void setLUTOptions(const timeline::LUTOptions&);

            //! Get the image options.
            const timeline::ImageOptions& getImageOptions() const;

            //! Observe the image options.
            std::shared_ptr<observer::IValue<timeline::ImageOptions> > observeImageOptions() const;

            //! Set the image options.
            void setImageOptions(const timeline::ImageOptions&);

            //! Get the display options.
            const timeline::DisplayOptions& getDisplayOptions() const;

            //! Observe the display options.
            std::shared_ptr<observer::IValue<timeline::DisplayOptions> > observeDisplayOptions() const;

            //! Set the display options.
            void setDisplayOptions(const timeline::DisplayOptions&);

        private:
            TLRENDER_PRIVATE();
        };
    }
}
