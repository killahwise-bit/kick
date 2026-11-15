#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace NoktaColours
{
    inline const juce::Colour cyan   { 0xff19e3ff };
    inline const juce::Colour purple { 0xff9a35ff };
    inline const juce::Colour lilac  { 0xffb46bff };
    inline const juce::Colour text   { 0xffd9d4ee };
}

class NoktaLookAndFeel : public juce::LookAndFeel_V4
{
public:
    NoktaLookAndFeel()
    {
        using namespace juce;
        setColour (ComboBox::textColourId, NoktaColours::cyan);
        setColour (ComboBox::arrowColourId, NoktaColours::cyan);
        setColour (PopupMenu::backgroundColourId, Colour (0xff0b0818));
        setColour (PopupMenu::textColourId, Colours::white);
        setColour (PopupMenu::highlightedBackgroundColourId, Colour (0xff2a1560));
        setColour (PopupMenu::highlightedTextColourId, NoktaColours::cyan);
        setColour (Slider::textBoxTextColourId, NoktaColours::cyan);
        setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
        setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
        setColour (Label::textColourId, NoktaColours::cyan);
    }

    juce::Font getLabelFont (juce::Label& l) override
    {
        return juce::Font (juce::FontOptions (juce::jmax (10.f, l.getHeight() * 0.78f)));
    }

    juce::Font getComboBoxFont (juce::ComboBox& b) override
    {
        return juce::Font (juce::FontOptions (b.getHeight() * 0.42f));
    }

    juce::Font getPopupMenuFont() override { return juce::Font (juce::FontOptions (18.f)); }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float a0, float a1, juce::Slider& s) override
    {
        using namespace juce;
        auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.f);
        const float d = jmin (b.getWidth(), b.getHeight());
        const auto c = b.getCentre();
        const float r = d * 0.5f, th = d * 0.09f;
        auto accent = s.findColour (Slider::rotarySliderFillColourId);
        if (! s.isEnabled()) accent = accent.withAlpha (0.3f);

        const float arcR = r - th * 0.5f;
        const float ang = a0 + pos * (a1 - a0);

        Path track;
        track.addCentredArc (c.x, c.y, arcR, arcR, 0.f, a0, a1, true);
        g.setColour (Colour (0xff1b1230));
        g.strokePath (track, PathStrokeType (th));

        Path val;
        val.addCentredArc (c.x, c.y, arcR, arcR, 0.f, a0, ang, true);
        g.setColour (accent.withAlpha (0.22f));
        g.strokePath (val, PathStrokeType (th * 2.2f));
        g.setColour (accent);
        g.strokePath (val, PathStrokeType (th));

        auto inner = Rectangle<float> (d - th * 2.4f, d - th * 2.4f).withCentre (c);
        g.setGradientFill (ColourGradient (Colour (0xff24212f), inner.getX() + inner.getWidth() * .35f,
                                           inner.getY() + inner.getHeight() * .28f, Colour (0xff07060c),
                                           inner.getRight(), inner.getBottom(), true));
        g.fillEllipse (inner);

        Path tick;
        tick.addRoundedRectangle (-1.5f, -inner.getHeight() * 0.5f + d * 0.05f, 3.f, d * 0.15f, 1.5f);
        tick.applyTransform (AffineTransform::rotation (ang).translated (c.x, c.y));
        g.setColour (Colour (0xffbff6ff).withAlpha (s.isEnabled() ? 1.f : 0.4f));
        g.fillPath (tick);
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        using namespace juce;
        auto r = Rectangle<float> (0.f, 0.f, (float) w, (float) h).reduced (1.f);
        g.setGradientFill (ColourGradient (Colour (0xff0b2433), 0.f, 0.f, Colour (0xff08141f), 0.f, (float) h, false));
        g.fillRoundedRectangle (r, 6.f);
        const auto col = box.isEnabled() ? NoktaColours::cyan : NoktaColours::cyan.withAlpha (0.35f);
        g.setColour (col);
        g.drawRoundedRectangle (r, 6.f, 1.f);
        const float ax = (float) w - h * 0.45f, ay = (float) h * 0.5f, k = h * 0.12f;
        Path arrow;
        arrow.addTriangle (ax - k * 1.6f, ay - k, ax + k * 1.6f, ay - k, ax, ay + k * 1.2f);
        g.fillPath (arrow);
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (14, 1, box.getWidth() - 50, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }
};
