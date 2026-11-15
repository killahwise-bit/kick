#pragma once
#include <functional>
#include "PluginProcessor.h"
#include "LookAndFeel.h"

// ---------------------------------------------------------------- Editor de curva
class CurveEditor : public juce::Component
{
public:
    explicit CurveEditor (NoktaPulseProcessor& p) : proc (p) { reload(); }

    void reload()
    {
        points = proc.getCurve();
        lastVersion = proc.getCurveVersion();
        repaint();
    }

    void tick()
    {
        if (dragIdx < 0 && curveSeg < 0 && proc.getCurveVersion() != lastVersion) reload();
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        const auto b = getLocalBounds().toFloat();
        const auto pl = plot();

        g.setColour (Colour (0xff0a0716));
        g.fillRoundedRectangle (b, 6.f);

        for (int i = 0; i <= 16; ++i)
        {
            g.setColour (Colour (i % 4 == 0 ? 0x4d9a6bff : 0x207a4fd0));
            g.drawVerticalLine (roundToInt (pl.getX() + pl.getWidth() * (float) i / 16.f), pl.getY(), pl.getBottom());
        }
        for (int i = 0; i <= 8; ++i)
        {
            g.setColour (Colour (0x207a4fd0));
            g.drawHorizontalLine (roundToInt (pl.getY() + pl.getHeight() * (float) i / 8.f), pl.getX(), pl.getRight());
        }

        // Forma de onda de la entrada
        {
            constexpr int n = NoktaPulseProcessor::numBins;
            const float cy = pl.getCentreY(), amp = pl.getHeight() * 0.45f;
            std::array<float, (size_t) n> vals;
            for (int i = 0; i < n; ++i) vals[(size_t) i] = jmin (1.f, proc.waveBins[(size_t) i].load()) * amp;
            Path w;
            for (int i = 0; i < n; ++i)
            {
                const float x = pl.getX() + pl.getWidth() * (float) i / (float) (n - 1);
                if (i == 0) w.startNewSubPath (x, cy - vals[0]); else w.lineTo (x, cy - vals[(size_t) i]);
            }
            for (int i = n - 1; i >= 0; --i)
                w.lineTo (pl.getX() + pl.getWidth() * (float) i / (float) (n - 1), cy + vals[(size_t) i]);
            w.closeSubPath();
            g.setColour (NoktaColours::cyan.withAlpha (0.16f));
            g.fillPath (w);
            g.setColour (NoktaColours::cyan.withAlpha (0.55f));
            g.strokePath (w, PathStrokeType (1.f));
        }

        // Curva
        Path line;
        constexpr int steps = 480;
        for (int i = 0; i <= steps; ++i)
        {
            const float x = (float) i / (float) steps;
            const auto sp = Point<float> (pl.getX() + x * pl.getWidth(),
                                          pl.getBottom() - CurveModel::evaluate (points, x) * pl.getHeight());
            if (i == 0) line.startNewSubPath (sp); else line.lineTo (sp);
        }
        Path fill (line);
        fill.lineTo (pl.getRight(), pl.getBottom());
        fill.lineTo (pl.getX(), pl.getBottom());
        fill.closeSubPath();
        g.setGradientFill (ColourGradient (Colour (0x997a22ee), 0.f, pl.getY(), Colour (0x083a0f80), 0.f, pl.getBottom(), false));
        g.fillPath (fill);
        g.setColour (NoktaColours::lilac);
        g.strokePath (line, PathStrokeType (3.5f));

        // Cursor de reproduccion
        const float ph = proc.displayPhase.load();
        const float px = pl.getX() + ph * pl.getWidth();
        g.setColour (NoktaColours::cyan);
        g.drawLine (px, pl.getY(), px, pl.getBottom(), 2.f);
        const float py = pl.getBottom() - CurveModel::evaluate (points, ph) * pl.getHeight();
        g.setColour (NoktaColours::cyan.withAlpha (0.25f));
        g.fillEllipse (px - 12.f, py - 12.f, 24.f, 24.f);

        // Nodos
        for (size_t i = 0; i < points.size(); ++i)
        {
            const auto s = toScreen (points[i]);
            const bool hot = (int) i == dragIdx || (int) i == hoverIdx;
            g.setColour (hot ? NoktaColours::cyan : NoktaColours::purple);
            g.fillEllipse (s.x - 8.f, s.y - 8.f, 16.f, 16.f);
            g.setColour (Colour (0xffe9d5ff));
            g.drawEllipse (s.x - 8.f, s.y - 8.f, 16.f, 16.f, 2.f);
        }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const int h = hitPoint (e.position);
        if (h != hoverIdx) { hoverIdx = h; repaint(); }
    }

    void mouseExit (const juce::MouseEvent&) override { hoverIdx = -1; repaint(); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        dragIdx = curveSeg = -1;
        const int idx = hitPoint (e.position);
        if (idx >= 0 && ! e.mods.isPopupMenu() && ! e.mods.isAltDown())
            dragIdx = idx;
        else if (e.mods.isPopupMenu() || e.mods.isAltDown())
        {
            const int seg = segmentAt (toNorm (e.position).x);
            if (seg >= 0) { curveSeg = seg; startC = points[(size_t) seg].c; startY = e.position.y; }
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragIdx >= 0)
        {
            auto n = toNorm (e.position);
            if (e.mods.isShiftDown())
            {
                n.x = std::round (n.x * 16.f) / 16.f;
                n.y = std::round (n.y * 8.f) / 8.f;
            }
            const size_t i = (size_t) dragIdx;
            if (i == 0) n.x = 0.f;
            else if (i == points.size() - 1) n.x = 1.f;
            else n.x = juce::jlimit (points[i - 1].x, points[i + 1].x, n.x);
            points[i].x = n.x;
            points[i].y = juce::jlimit (0.f, 1.f, n.y);
            commit();
        }
        else if (curveSeg >= 0)
        {
            const size_t s = (size_t) curveSeg;
            const float dy = (startY - e.position.y) / plot().getHeight();
            const bool rising = points[s + 1].y >= points[s].y;
            points[s].c = juce::jlimit (-1.5f, 1.5f, startC + (rising ? -dy : dy) * 3.f);
            commit();
        }
    }

    void mouseUp (const juce::MouseEvent&) override { dragIdx = curveSeg = -1; repaint(); }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const int idx = hitPoint (e.position);
        if (idx > 0 && idx < (int) points.size() - 1 && points.size() > 2)
            points.erase (points.begin() + idx);
        else if (idx < 0)
        {
            const auto n = toNorm (e.position);
            points.push_back ({ n.x, n.y, 0.f });
            CurveModel::sortPoints (points);
        }
        else return;
        commit();
    }

private:
    juce::Rectangle<float> plot() const { return getLocalBounds().toFloat().reduced (14.f, 16.f); }

    juce::Point<float> toScreen (const CurvePoint& p) const
    {
        const auto pl = plot();
        return { pl.getX() + p.x * pl.getWidth(), pl.getBottom() - p.y * pl.getHeight() };
    }

    juce::Point<float> toNorm (juce::Point<float> s) const
    {
        const auto pl = plot();
        return { juce::jlimit (0.f, 1.f, (s.x - pl.getX()) / pl.getWidth()),
                 juce::jlimit (0.f, 1.f, (pl.getBottom() - s.y) / pl.getHeight()) };
    }

    int hitPoint (juce::Point<float> pos) const
    {
        for (int i = (int) points.size() - 1; i >= 0; --i)
            if (toScreen (points[(size_t) i]).getDistanceFrom (pos) < 12.f) return i;
        return -1;
    }

    int segmentAt (float x) const
    {
        for (size_t i = 0; i + 1 < points.size(); ++i)
            if (x >= points[i].x && x <= points[i + 1].x && points[i + 1].x > points[i].x) return (int) i;
        return -1;
    }

    void commit()
    {
        proc.setCurve (points);
        lastVersion = proc.getCurveVersion();
        repaint();
    }

    NoktaPulseProcessor& proc;
    std::vector<CurvePoint> points;
    int lastVersion = -1, dragIdx = -1, hoverIdx = -1, curveSeg = -1;
    float startC = 0.f, startY = 0.f;
};

// ---------------------------------------------------------------- Barra de tiempo
class TimelineStrip : public juce::Component
{
public:
    explicit TimelineStrip (NoktaPulseProcessor& p) : proc (p) {}

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        const auto b = getLocalBounds().toFloat();
        g.setColour (Colour (0xff0a0716));
        g.fillRoundedRectangle (b, 6.f);
        g.setColour (Colour (0xff3a2570));
        g.drawRoundedRectangle (b.reduced (0.5f), 6.f, 1.f);

        const auto tr = Rectangle<float> (b.getWidth() - 28.f, b.getHeight() * 0.22f).withCentre (b.getCentre());
        const float ph = proc.displayPhase.load();
        g.setColour (Colour (0xff1b1230));
        g.fillRoundedRectangle (tr, tr.getHeight() / 2.f);
        g.setColour (NoktaColours::purple);
        g.fillRoundedRectangle (tr.withWidth (jmax (tr.getHeight(), tr.getWidth() * ph)), tr.getHeight() / 2.f);
        g.setColour (NoktaColours::cyan);
        g.fillEllipse (Rectangle<float> (tr.getHeight() * 1.8f, tr.getHeight() * 1.8f)
                           .withCentre ({ tr.getX() + tr.getWidth() * ph, tr.getCentreY() }));

        g.setFont (b.getHeight() * 0.28f);
        for (int i = 0; i < 4; ++i)
        {
            const float x = tr.getX() + tr.getWidth() * (float) i / 4.f;
            g.setColour (Colour (0x809a6bff));
            if (i > 0) g.drawVerticalLine (roundToInt (x), b.getY() + 6.f, b.getBottom() - 6.f);
            g.setColour (Colour (0xffa79fc9));
            g.drawText (String (i + 1), Rectangle<float> (x + 6.f, b.getY() + 3.f, 30.f, b.getHeight() * 0.35f),
                        Justification::centredLeft);
        }
    }

private:
    NoktaPulseProcessor& proc;
};

// ---------------------------------------------------------------- Medidor vertical
class MeterBar : public juce::Component
{
public:
    explicit MeterBar (std::function<float()> f) : getter (std::move (f)) {}

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        const auto r = getLocalBounds().toFloat();
        g.setColour (Colour (0xff05040a));
        g.fillRoundedRectangle (r, 5.f);
        g.setColour (Colour (0xff2a1a55));
        g.drawRoundedRectangle (r.reduced (0.5f), 5.f, 1.f);
        auto f = r.reduced (3.f);
        f = f.removeFromBottom (f.getHeight() * jlimit (0.f, 1.f, getter()));
        g.setGradientFill (ColourGradient (NoktaColours::cyan, 0.f, r.getY(), NoktaColours::purple, 0.f, r.getBottom(), false));
        g.fillRoundedRectangle (f, 3.f);
    }

private:
    std::function<float()> getter;
};

// ---------------------------------------------------------------- Preset de curva
class PresetTile : public juce::Component
{
public:
    explicit PresetTile (int idx) : index (idx), pts (CurveModel::preset (idx)) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }

    std::function<void (int)> onClick;

    void setActive (bool a) { if (a != active) { active = a; repaint(); } }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        const auto b = getLocalBounds().toFloat().reduced (1.f);
        g.setGradientFill (ColourGradient (Colour (0xff0d0a1a), b.getX(), b.getY(), Colour (0xff07050f), b.getRight(), b.getBottom(), false));
        g.fillRoundedRectangle (b, 10.f);
        g.setColour (active ? NoktaColours::cyan : Colour (0xff4a2a9a));
        g.drawRoundedRectangle (b, 10.f, active ? 2.f : 1.f);

        const auto a = b.reduced (b.getWidth() * 0.15f, b.getHeight() * 0.22f);
        Path p;
        for (int i = 0; i <= 80; ++i)
        {
            const float x = (float) i / 80.f;
            const Point<float> s (a.getX() + x * a.getWidth(), a.getBottom() - CurveModel::evaluate (pts, x) * a.getHeight());
            if (i == 0) p.startNewSubPath (s); else p.lineTo (s);
        }
        g.setColour (active ? NoktaColours::cyan : NoktaColours::lilac);
        g.strokePath (p, PathStrokeType (3.f));
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (getLocalBounds().contains (e.getPosition()) && onClick) onClick (index);
    }

private:
    int index;
    std::vector<CurvePoint> pts;
    bool active = false;
};
