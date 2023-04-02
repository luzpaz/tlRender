// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2021-2023 Darby Johnston
// All rights reserved.

#include <tlUI/FloatEdit.h>

#include <tlUI/DrawUtil.h>

#include <tlCore/StringFormat.h>

namespace tl
{
    namespace ui
    {
        struct FloatEdit::Private
        {
            std::shared_ptr<FloatModel> model;
            std::string text;
            std::string format;
            int digits = 3;
            int precision = 2;
            imaging::FontInfo fontInfo;
            math::Vector2i textSize;
            math::Vector2i formatSize;
            int lineHeight = 0;
            int ascender = 0;
            int margin = 0;
            int border = 0;
            std::shared_ptr<observer::ValueObserver<float> > valueObserver;
        };

        void FloatEdit::_init(
            const std::shared_ptr<system::Context>& context,
            const std::shared_ptr<IWidget>& parent)
        {
            IWidget::_init("tl::ui::FloatEdit", context, parent);
            TLRENDER_P();
            setModel(FloatModel::create(context));
            p.fontInfo.family = "NotoMono-Regular";
        }

        FloatEdit::FloatEdit() :
            _p(new Private)
        {}

        FloatEdit::~FloatEdit()
        {}

        std::shared_ptr<FloatEdit> FloatEdit::create(
            const std::shared_ptr<system::Context>& context,
            const std::shared_ptr<IWidget>& parent)
        {
            auto out = std::shared_ptr<FloatEdit>(new FloatEdit);
            out->_init(context, parent);
            return out;
        }

        const std::shared_ptr<FloatModel>& FloatEdit::getModel() const
        {
            return _p->model;
        }

        void FloatEdit::setModel(const std::shared_ptr<FloatModel>& value)
        {
            TLRENDER_P();
            p.valueObserver.reset();
            p.model = value;
            if (p.model)
            {
                p.valueObserver = observer::ValueObserver<float>::create(
                    p.model->observeValue(),
                    [this](float)
                    {
                        _textUpdate();
                    });
            }
            _textUpdate();
        }

        void FloatEdit::setDigits(int value)
        {
            TLRENDER_P();
            if (value == p.digits)
                return;
            p.digits = value;
            _textUpdate();
            _updates |= Update::Size;
            _updates |= Update::Draw;
        }

        void FloatEdit::setPrecision(int value)
        {
            TLRENDER_P();
            if (value == p.precision)
                return;
            p.precision = value;
            _textUpdate();
            _updates |= Update::Size;
            _updates |= Update::Draw;
        }

        void FloatEdit::setFontInfo(const imaging::FontInfo& value)
        {
            TLRENDER_P();
            if (value == p.fontInfo)
                return;
            p.fontInfo = value;
            _updates |= Update::Size;
            _updates |= Update::Draw;
        }

        void FloatEdit::sizeEvent(const SizeEvent& event)
        {
            IWidget::sizeEvent(event);
            TLRENDER_P();

            p.margin = event.style->getSizeRole(SizeRole::MarginInside) * event.contentScale;
            p.border = event.style->getSizeRole(SizeRole::Border) * event.contentScale;

            auto fontInfo = p.fontInfo;
            fontInfo.size *= event.contentScale;
            p.textSize = event.fontSystem->measure(p.text, fontInfo);
            auto fontMetrics = event.fontSystem->getMetrics(fontInfo);
            p.lineHeight = fontMetrics.lineHeight;
            p.ascender = fontMetrics.ascender;
            p.formatSize = event.fontSystem->measure(p.format, fontInfo);

            _sizeHint.x = p.formatSize.x + p.margin * 2;
            _sizeHint.y = p.lineHeight + p.margin * 2;
        }

        void FloatEdit::drawEvent(const DrawEvent& event)
        {
            IWidget::drawEvent(event);
            TLRENDER_P();

            math::BBox2i g = _geometry;

            event.render->drawMesh(
                border(g, p.border),
                event.style->getColorRole(ColorRole::Border));

            event.render->drawRect(
                g.margin(-p.border),
                event.style->getColorRole(ColorRole::Base));

            math::BBox2i g2 = g.margin(-p.margin);
            auto fontInfo = p.fontInfo;
            fontInfo.size *= event.contentScale;
            event.render->drawText(
                event.fontSystem->getGlyphs(p.text, fontInfo),
                math::Vector2i(g2.x() + g2.w() - p.textSize.x, g2.y() + p.ascender),
                event.style->getColorRole(ColorRole::Text));
        }

        void FloatEdit::_textUpdate()
        {
            TLRENDER_P();
            std::string text;
            if (p.model)
            {
                text = string::Format("{0}").arg(p.model->getValue(), p.precision);
            }
            p.text = text;
            p.format = string::Format("{0}").arg(0.F, p.precision, p.digits);
        }
    }
}
