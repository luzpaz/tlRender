// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2021-2024 Darby Johnston
// All rights reserved.

#pragma once

#include <tlQt/TimelinePlayer.h>

#include <tlTimeline/BackgroundOptions.h>
#include <tlTimeline/CompareOptions.h>
#include <tlTimeline/DisplayOptions.h>
#include <tlTimeline/LUTOptions.h>
#include <tlTimeline/OCIOOptions.h>

#include <QOpenGLWidget>
#include <QOpenGLFunctions_4_1_Core>
#include <QSharedPointer>
#include <QVector>

namespace tl
{
    namespace qtwidget
    {
        //! Timeline viewport widget.
        class TimelineViewport :
            public QOpenGLWidget,
            protected QOpenGLFunctions_4_1_Core
        {
            Q_OBJECT

        public:
            TimelineViewport(
                const std::shared_ptr<system::Context>&,
                QWidget* parent = nullptr);

            virtual ~TimelineViewport();

            //! Set the background options.
            void setBackgroundOptions(const timeline::BackgroundOptions&);

            //! Set the OpenColorIO options.
            void setOCIOOptions(const timeline::OCIOOptions&);

            //! Set the LUT options.
            void setLUTOptions(const timeline::LUTOptions&);

            //! Set the image options.
            void setImageOptions(const std::vector<timeline::ImageOptions>&);

            //! Set the display options.
            void setDisplayOptions(const std::vector<timeline::DisplayOptions>&);

            //! Set the comparison options.
            void setCompareOptions(const timeline::CompareOptions&);

            //! Set the timeline players.
            void setTimelinePlayers(const QVector<QSharedPointer<qt::TimelinePlayer> >&);

            //! Get the view position.
            const math::Vector2i& viewPos() const;

            //! Get the view zoom.
            double viewZoom() const;

            //! Get whether the view is framed.
            bool hasFrameView() const;

        public Q_SLOTS:
            //! Set the view position and zoom.
            void setViewPosAndZoom(const tl::math::Vector2i&, double);

            //! Set the view zoom.
            void setViewZoom(double, const tl::math::Vector2i& focus = tl::math::Vector2i());

            //! Frame the view.
            void setFrameView(bool);

            //! Set the view zoom to 1:1.
            void viewZoom1To1();

            //! Zoom the view in.
            void viewZoomIn();

            //! Zoom the view out.
            void viewZoomOut();

        Q_SIGNALS:
            //! This signal is emitted when the comparison options are changed.
            void compareOptionsChanged(const tl::timeline::CompareOptions&);

            //! This signal is emitted when the position and zoom change.
            void viewPosAndZoomChanged(const tl::math::Vector2i&, double);

            //! This signal is emitted when the frame view is changed.
            void frameViewChanged(bool);

            //! This signal is emitted when the dropped frames count is changed.
            void droppedFramesChanged(bool);

        private Q_SLOTS:
            void _playbackUpdate(timeline::Playback);
            void _videoDataUpdate(const tl::timeline::VideoData&);

        protected:
            void initializeGL() override;
            void resizeGL(int w, int h) override;
            void paintGL() override;
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
            void enterEvent(QEvent*) override;
#else
            void enterEvent(QEnterEvent*) override;
#endif // QT_VERSION
            void leaveEvent(QEvent*) override;
            void mousePressEvent(QMouseEvent*) override;
            void mouseReleaseEvent(QMouseEvent*) override;
            void mouseMoveEvent(QMouseEvent*) override;
            void wheelEvent(QWheelEvent*) override;
            void keyPressEvent(QKeyEvent*) override;

        private:
            math::Size2i _viewportSize() const;
            math::Size2i _renderSize() const;
            math::Vector2i _viewportCenter() const;
            void _frameView();

            void _droppedFramesUpdate(const otime::RationalTime&);

            TLRENDER_PRIVATE();
        };
    }
}
