#pragma once
#include <juce_core/juce_core.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

// Punto de la curva. x: posicion en el ciclo (0..1), y: ganancia (0 = silencio, 1 = sin atenuacion).
// c: curvatura del tramo que sale de este punto hacia el siguiente (-1.5..1.5, 0 = recta).
struct CurvePoint
{
    float x = 0.f, y = 0.f, c = 0.f;
};

namespace CurveModel
{
    constexpr int tableSize  = 1024;
    constexpr int numPresets = 7;
    using Table = std::array<float, (size_t) tableSize + 1>;

    inline void sortPoints (std::vector<CurvePoint>& p)
    {
        std::stable_sort (p.begin(), p.end(),
                          [] (const CurvePoint& a, const CurvePoint& b) { return a.x < b.x; });
    }

    inline float evaluate (const std::vector<CurvePoint>& p, float x)
    {
        if (p.empty()) return 1.f;
        x = juce::jlimit (0.f, 1.f, x);
        if (x <= p.front().x) return p.front().y;

        for (size_t i = 0; i + 1 < p.size(); ++i)
        {
            const auto& a = p[i];
            const auto& b = p[i + 1];
            if (x < b.x)
            {
                const float dx = b.x - a.x;
                if (dx <= 1.0e-6f) return b.y;
                const float u = (x - a.x) / dx;
                const float k = std::pow (4.f, a.c);
                return a.y + (b.y - a.y) * std::pow (u, k);
            }
        }
        return p.back().y;
    }

    inline void buildTable (const std::vector<CurvePoint>& p, Table& t)
    {
        for (int i = 0; i <= tableSize; ++i)
            t[(size_t) i] = evaluate (p, (float) i / (float) tableSize);
    }

    inline std::vector<CurvePoint> preset (int index)
    {
        switch (index)
        {
            default:
            case 0: return { { 0.f, .27f, 0.f }, { .10f, 0.f, -.45f }, { .72f, 1.f, 0.f }, { 1.f, 1.f, 0.f } };
            case 1: return { { 0.f, 0.f, 0.f }, { 1.f, 1.f, 0.f } };
            case 2: return { { 0.f, .1f, 0.f }, { .33f, .1f, 0.f }, { .33f, .55f, 0.f },
                             { .66f, .55f, 0.f }, { .66f, 1.f, 0.f }, { 1.f, 1.f, 0.f } };
            case 3: return { { 0.f, 0.f, -.9f }, { .6f, 1.f, 0.f }, { 1.f, 1.f, 0.f } };
            case 4: return { { 0.f, 0.f, 0.f }, { .15f, 0.f, 0.f }, { .15f, 1.f, 0.f },
                             { .85f, 1.f, 0.f }, { .85f, 0.f, 0.f }, { 1.f, 0.f, 0.f } };
            case 5: return { { 0.f, .1f, 0.f }, { .8f, 1.f, 0.f }, { 1.f, 1.f, 0.f } };
            case 6: return { { 0.f, 1.f, .7f }, { 1.f, .05f, 0.f } };
        }
    }

    inline bool equal (const std::vector<CurvePoint>& a, const std::vector<CurvePoint>& b)
    {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i)
            if (std::abs (a[i].x - b[i].x) > 1e-4f || std::abs (a[i].y - b[i].y) > 1e-4f
                || std::abs (a[i].c - b[i].c) > 1e-4f)
                return false;
        return true;
    }
}
