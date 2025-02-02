// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2021-2024 Darby Johnston
// All rights reserved.

#pragma once

#include <tlTimelineUI/IItem.h>

#include <tlTimeline/Player.h>

namespace tl
{
    namespace timelineui
    {
        //! Track types.
        enum class TrackType
        {
            None,
            Video,
            Audio
        };

        //! Timeline item.
        //!
        //! \todo Add a selection model.
        //! \todo Add support for dragging clips to different tracks.
        //! \todo Add support for adjusting clip handles.
        //! \todo Add support for undo/redo.
        //! \todo Add an option for viewing/playing indiviual clips ("solo" mode).
        class TimelineItem : public IItem
        {
        protected:
            void _init(
                const std::shared_ptr<timeline::Player>&,
                const otio::SerializableObject::Retainer<otio::Stack>&,
                double scale,
                const ItemOptions&,
                const std::shared_ptr<ItemData>&,
                const std::shared_ptr<system::Context>&,
                const std::shared_ptr<IWidget>& parent);

            TimelineItem();

        public:
            virtual ~TimelineItem();

            //! Create a new item.
            static std::shared_ptr<TimelineItem> create(
                const std::shared_ptr<timeline::Player>&,
                const otio::SerializableObject::Retainer<otio::Stack>&,
                double scale,
                const ItemOptions&,
                const std::shared_ptr<ItemData>&,
                const std::shared_ptr<system::Context>&,
                const std::shared_ptr<IWidget>& parent = nullptr);

            //! Set whether the timeline is editable.
            void setEditable(bool);

            //! Set whether playback stops when scrubbing.
            void setStopOnScrub(bool);

            void setOptions(const ItemOptions&) override;

            void setGeometry(const math::Box2i&) override;
            void sizeHintEvent(const ui::SizeHintEvent&) override;
            void drawOverlayEvent(const math::Box2i&, const ui::DrawEvent&) override;
            void mouseMoveEvent(ui::MouseMoveEvent&) override;
            void mousePressEvent(ui::MouseClickEvent&) override;
            void mouseReleaseEvent(ui::MouseClickEvent&) override;
            //void keyPressEvent(ui::KeyEvent&) override;
            //void keyReleaseEvent(ui::KeyEvent&) override;

        protected:
            void _timeUnitsUpdate() override;

            void _releaseMouse() override;

        private:
            void _drawInOutPoints(
                const math::Box2i&,
                const ui::DrawEvent&);
            void _drawTimeTicks(
                const math::Box2i&,
                const ui::DrawEvent&);
            void _drawCacheInfo(
                const math::Box2i&,
                const ui::DrawEvent&);
            void _drawCurrentTime(
                const math::Box2i&,
                const ui::DrawEvent&);

            void _textUpdate();

            TLRENDER_PRIVATE();
        };
    }
}
