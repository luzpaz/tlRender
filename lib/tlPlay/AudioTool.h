// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2021-2022 Darby Johnston
// All rights reserved.

#pragma once

#include <tlPlay/ToolWidget.h>

namespace tl
{
    namespace play
    {
        //! Audio offset widget.
        class AudioOffsetWidget : public QWidget
        {
            Q_OBJECT

        public:
            AudioOffsetWidget(QWidget* parent = nullptr);

        public Q_SLOTS:
            void setAudioOffset(double);

        Q_SIGNALS:
            void offsetChanged(double);

        private Q_SLOTS:
            void _sliderCallback(int);
            void _spinBoxCallback(double);
            void _resetCallback();

        private:
            void _offsetUpdate();

            TLRENDER_PRIVATE();
        };

        //! Audio tool.
        class AudioTool : public ToolWidget
        {
            Q_OBJECT

        public:
            AudioTool(QWidget* parent = nullptr);

        public Q_SLOTS:
            void setAudioOffset(double);

        Q_SIGNALS:
            void audioOffsetChanged(double);

        private:
            TLRENDER_PRIVATE();
        };
    }
}
