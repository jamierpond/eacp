#pragma once

// Small shared UI pieces for the Hub demo windows. eacp ships a TextInput
// widget but no button, so here's a minimal one built from a rounded
// shape layer plus a text layer.

#include <eacp/Graphics/Graphics.h>

#include <algorithm>
#include <functional>
#include <string>

namespace hub::ui
{

// Palette shared by both windows.
inline const eacp::Graphics::Color background {0.09f, 0.10f, 0.13f};
inline const eacp::Graphics::Color panel {0.15f, 0.16f, 0.20f};
inline const eacp::Graphics::Color accent {0.30f, 0.55f, 1.00f};
inline const eacp::Graphics::Color good {0.25f, 0.80f, 0.45f};
inline const eacp::Graphics::Color bad {0.95f, 0.35f, 0.40f};
inline const eacp::Graphics::Color text {0.92f, 0.93f, 0.96f};
inline const eacp::Graphics::Color faint {0.55f, 0.57f, 0.63f};

struct Button final : eacp::Graphics::View
{
    explicit Button(const std::string& labelText)
    {
        using namespace eacp::Graphics;

        getProperties().handlesMouseEvents = true;

        textLength = labelText.size();
        label->setText(labelText);
        label->setColor({1.f, 1.f, 1.f});
        label->setFont(FontOptions().withName("Helvetica-Bold").withSize(15.f));

        addChildren({backgroundLayer, label});
        applyColor();
    }

    void setColor(const eacp::Graphics::Color& color)
    {
        baseColor = color;
        applyColor();
    }

    void setText(const std::string& labelText)
    {
        textLength = labelText.size();
        label->setText(labelText);
        resized();
    }

    void setEnabled(bool value)
    {
        enabled = value;
        applyColor();
    }

    void mouseEntered(const eacp::Graphics::MouseEvent&) override
    {
        hovering = true;
        applyColor();
    }

    void mouseExited(const eacp::Graphics::MouseEvent&) override
    {
        hovering = false;
        applyColor();
    }

    void mouseDown(const eacp::Graphics::MouseEvent&) override
    {
        if (enabled)
            onClick();
    }

    void resized() override
    {
        using namespace eacp::Graphics;

        auto bounds = getLocalBounds();
        auto path = Path();
        path.addRoundedRect(bounds, 9.f);
        backgroundLayer->setPath(path);
        scaleToFit({backgroundLayer});

        // No text metrics available; approximate horizontal centering from
        // the glyph count so labels sit roughly in the middle.
        auto width = 8.4f * static_cast<float>(labelLength());
        label->setPosition(
            {std::max(12.f, (bounds.w - width) / 2.f), bounds.h / 2.f - 9.f});
    }

    std::function<void()> onClick = [] {};

private:
    size_t labelLength() const { return textLength; }

    void applyColor()
    {
        using namespace eacp::Graphics;

        auto color = enabled ? baseColor : Color {0.3f, 0.32f, 0.36f};
        if (enabled && hovering)
            color = color.withAlpha(0.82f);

        backgroundLayer->setFillColor(color);
    }

    eacp::Graphics::Color baseColor = accent;
    bool hovering = false;
    bool enabled = true;
    size_t textLength = 0;

    eacp::Graphics::ShapeLayerView backgroundLayer;
    eacp::Graphics::TextLayerView label;
};

} // namespace hub::ui
