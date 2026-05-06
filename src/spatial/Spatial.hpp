#pragma once

#include <limits>
#include <vector>

using u64 = std::uint64_t;
using u32 = std::uint32_t;
using u16 = std::uint16_t;
using u8  = std::uint8_t;

using f32 = float;
using f64 = double;

template<typename T>
struct Arena {
    std::vector<T> pool;
    u32 used = 0;

    void reserveCapacity(u32 cap) { pool.resize(cap); }

    void reset() noexcept { used = 0; }

    u32 alloc(u32 count = 1) noexcept {
        assert(used + count <= pool.size() && "Arena overflow");
        u32 idx = used;
        used += count;
        return idx;
    }

    T& operator[](u32 i) noexcept { return pool[i]; }
    const T& operator[](u32 i) const noexcept { return pool[i]; }
};

template<typename Float>
struct Vec2Base { Float x, y; };

template<typename T, typename Float>
concept HasXY = requires(T v) {
    { v.x } -> std::convertible_to<Float>;
    { v.y } -> std::convertible_to<Float>;
};

template<typename Float, typename Vec2 = Vec2Base<Float>>
requires HasXY<Vec2, Float>
struct AABB2D {
    Float minX, minY, maxX, maxY;

    bool contains(Vec2 p) const noexcept {
        return p.x >= minX && p.x <= maxX && p.y >= minY && p.y <= maxY;
    }
    bool overlaps(const AABB2D& o) const noexcept {
        return minX <= o.maxX && maxX >= o.minX &&
                minY <= o.maxY && maxY >= o.minY;
    }

    Vec2 center() const noexcept {
        return { (minX + maxX) * Float(0.5), (minY + maxY) * Float(0.5) };
    }

    Float minDist2(Vec2 p) const noexcept {
        Float dx = std::max({ minX - p.x, Float(0), p.x - maxX });
        Float dy = std::max({ minY - p.y, Float(0), p.y - maxY });
        return dx*dx + dy*dy;
    }

    static AABB2D inf()
    {
        constexpr Float max = std::numeric_limits<Float>::max();
        return AABB2D{-max, -max, max, max};
    }
};