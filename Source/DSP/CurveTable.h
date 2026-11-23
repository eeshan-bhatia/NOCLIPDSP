#pragma once
#include <array>
#include <algorithm>
#include <atomic>
#include <cmath>

namespace noclip
{
using CurvePoints = std::array<float, 9>;
inline constexpr CurvePoints identityPoints { -1, -.75f, -.5f, -.25f, 0, .25f, .5f, .75f, 1 };
struct CurveTable
{
    static constexpr size_t size = 2049;
    std::array<float, size> a {}, b {};
    CurvePoints sourceA=identityPoints, sourceB=identityPoints;
    static std::array<float, size> build(CurvePoints points) noexcept
    {
        for (auto& p : points) p = std::isfinite(p) ? std::clamp(p, -1.0f, 1.0f) : 0;
        std::array<float, 8> slope {};
        std::array<float, 9> tangent {};
        for (size_t i = 0; i < 8; ++i) slope[i] = points[i + 1] - points[i];
        tangent[0] = slope[0]; tangent[8] = slope[7];
        for (size_t i = 1; i < 8; ++i)
            tangent[i] = slope[i - 1] * slope[i] <= 0 ? 0
                : 2 * slope[i - 1] * slope[i] / (slope[i - 1] + slope[i]);
        std::array<float, size> result;
        for (size_t i = 0; i < size; ++i)
        {
            const float pos = static_cast<float>(i) * 8 / static_cast<float>(size - 1);
            const size_t j = std::min(size_t(7), static_cast<size_t>(pos));
            const float u = pos - static_cast<float>(j), u2 = u * u, u3 = u2 * u;
            const float y = (2*u3-3*u2+1)*points[j] + (u3-2*u2+u)*tangent[j]
                          + (-2*u3+3*u2)*points[j+1] + (u3-u2)*tangent[j+1];
            result[i] = std::clamp(y, std::min(points[j], points[j+1]), std::max(points[j], points[j+1]));
        }
        return result;
    }
    static CurveTable make(CurvePoints aPoints = identityPoints, CurvePoints bPoints = identityPoints) noexcept
    { return { build(aPoints), build(bPoints), aPoints, bPoints }; }
    static float evaluate(const std::array<float, size>& table, double x) noexcept
    {
        if(!std::isfinite(x)) return 0;
        const double pos = (std::clamp(x, -1.0, 1.0) + 1) * 0.5 * static_cast<double>(size - 1);
        const auto i = std::min(size - 2, static_cast<size_t>(pos));
        const float frac = static_cast<float>(pos - static_cast<double>(i));
        return table[i] + frac * (table[i+1] - table[i]);
    }
};

// Single worker producer / single audio consumer. Ownership changes via exchange:
// neither thread writes the other's slot; no locks, allocation or destruction.
class CurveMailbox
{
public:
    void publish(const CurveTable& table) noexcept
    {
        slots[back] = table;
        back = middle.exchange(back | dirty, std::memory_order_acq_rel) & mask;
    }
    bool consume(CurveTable& table) noexcept
    {
        if ((middle.load(std::memory_order_acquire) & dirty) == 0) return false;
        front = middle.exchange(front, std::memory_order_acq_rel) & mask;
        table = slots[front];
        return true;
    }
private:
    static constexpr unsigned dirty = 4, mask = 3;
    std::array<CurveTable, 3> slots;
    std::atomic<unsigned> middle { 1 };
    unsigned front = 0, back = 2;
    static_assert(std::atomic<unsigned>::is_always_lock_free);
};
}
