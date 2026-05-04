#pragma once

#include <array>
#include <cassert>
#include <cstdint>
#include <limits>
#include <vector>
#include <algorithm>

using u64 = std::uint64_t;
using u32 = std::uint32_t;
using u16 = std::uint16_t;
using u8  = std::uint8_t;

template<typename Float>
struct Vec2Base { Float x, y; };

template<typename T, typename Float>
concept HasXY = requires(T v) {
    { v.x } -> std::convertible_to<Float>;
    { v.y } -> std::convertible_to<Float>;
};

// ─────────────────────────────────────────────────────────────────────────────
//  QuadTree — hot/cold node split
//
//  Проблема предыдущей версии:
//    Node хранил и children_id, и items_id[CAPACITY] вместе.
//    При большом CAPACITY или Int_Node=u64 Node > 64 байт → 2+ cache-line.
//
//  Решение:
//    NodeCore  — «горячая» часть, всегда ≤ 64 байт.
//                bounds + children_id + смещение/счётчик в item_pool_.
//    item_pool_ — отдельный flat-вектор Int_Item;
//                 items одного листа лежат ПОДРЯД → один prefetch накрывает всё.
//
//  Компромисс:
//    Вставка в лист: append в конец item_pool_ (amortized O(1)).
//    Split: items текущего листа уже contiguous — читаем их пачкой,
//           перераспределяем по детям, освобождаем слот (swap-with-end в пуле).
//    Удаление: O(itemCount) поиск + swap-with-end в пуле.
// ─────────────────────────────────────────────────────────────────────────────
template<
    typename T        = std::uint64_t,
    typename Float    = float,
    typename Vec2     = Vec2Base<Float>,
    typename Int_Node = std::uint32_t,   // индекс узла
    typename Int_Item = std::uint32_t,   // индекс в item_pool_
    u64 MAX_DEPTH     = 16,
    u64 CAPACITY      = 8               // макс. items на лист до split
>
requires HasXY<Vec2, Float>
struct QuadTree {

    // ── AABB ─────────────────────────────────────────────────────────────────
    struct AABB {
        Float minX, minY, maxX, maxY;

        [[nodiscard]] bool contains(Vec2 p) const noexcept {
            return p.x >= minX && p.x <= maxX &&
                   p.y >= minY && p.y <= maxY;
        }
        [[nodiscard]] bool intersects(const AABB& o) const noexcept {
            return minX <= o.maxX && maxX >= o.minX &&
                   minY <= o.maxY && maxY >= o.minY;
        }
        [[nodiscard]] Vec2 center() const noexcept {
            return { (minX + maxX) * Float(0.5),
                     (minY + maxY) * Float(0.5) };
        }
        [[nodiscard]] Float minDist2(Vec2 p) const noexcept {
            Float dx = std::max({ minX - p.x, Float(0), p.x - maxX });
            Float dy = std::max({ minY - p.y, Float(0), p.y - maxY });
            return dx * dx + dy * dy;
        }
    };

    struct Item {
        Vec2 pos;
        T    data;
    };

    struct NodeCore {
        AABB     bounds {};
        std::array<Int_Node, 4> children_id {};
        Int_Item pool_offset { NULL_ITEM }; 
        u8       item_count  { 0 };

        NodeCore() noexcept { children_id.fill(NULL_NODE); }

        [[nodiscard]] bool isLeaf() const noexcept {
            return children_id[0] == NULL_NODE;
        }
    };

    static constexpr Int_Node NULL_NODE = std::numeric_limits<Int_Node>::max();
    static constexpr Int_Item NULL_ITEM = std::numeric_limits<Int_Item>::max();

    explicit QuadTree(AABB bounds) noexcept { clear(bounds); }

    void clear(AABB bounds) noexcept {
        nodes_.clear();
        items_.clear();
        item_pool_.clear();
        root_ = allocNode(bounds);
    }

    void reserve(std::size_t nodes, std::size_t items) {
        nodes_.reserve(nodes);
        items_.reserve(items);
        item_pool_.reserve(items);
    }

    bool insert(Vec2 pos, T data) noexcept {
        if (!nodes_[root_].bounds.contains(pos)) return false;
        return insertInto(root_, pos, std::move(data), 0);
    }

    template<typename Callback>
    void query(const AABB& range, Callback&& cb) const noexcept {
        queryNode(root_, range, std::forward<Callback>(cb));
    }

    [[nodiscard]] const Item* nearest(Vec2 target) const noexcept {
        const Item* best   = nullptr;
        Float       bestD2 = std::numeric_limits<Float>::max();
        nearestNode(root_, target, best, bestD2);
        return best;
    }

    template<typename Pred>
    bool remove(Pred&& pred) noexcept {
        return removeFrom(root_, std::forward<Pred>(pred));
    }

    [[nodiscard]] std::size_t nodeCount() const noexcept { return nodes_.size(); }
    [[nodiscard]] std::size_t itemCount() const noexcept { return items_.size(); }

protected:
    Int_Node root_ { 0 };

    std::vector<NodeCore> nodes_;
    std::vector<Item>     items_;

    std::vector<Int_Item> item_pool_;

    Int_Node allocNode(const AABB& b) noexcept {
        nodes_.emplace_back();
        nodes_.back().bounds = b;
        return static_cast<Int_Node>(nodes_.size() - 1);
    }

    Int_Item allocItem(Vec2 pos, T&& data) noexcept {
        items_.push_back({ pos, std::move(data) });
        return static_cast<Int_Item>(items_.size() - 1);
    }

    void pushItemToLeaf(Int_Node ni, Int_Item item_id) noexcept {
        NodeCore& n = nodes_[ni];
        if (n.pool_offset == NULL_ITEM) {
            n.pool_offset = static_cast<Int_Item>(item_pool_.size());
        }
        item_pool_.push_back(item_id);
        ++n.item_count;
    }

    static void childAABBs(const AABB& b, std::array<AABB, 4>& out) noexcept {
        const Vec2 c = b.center();
        out[0] = { b.minX, b.minY, c.x,    c.y    }; // SW
        out[1] = { c.x,    b.minY, b.maxX, c.y    }; // SE
        out[2] = { b.minX, c.y,    c.x,    b.maxY }; // NW
        out[3] = { c.x,    c.y,    b.maxX, b.maxY }; // NE
    }

    static int quadrantOf(const AABB& b, Vec2 p) noexcept {
        const Vec2 c = b.center();
        return (p.x >= c.x ? 1 : 0) | (p.y >= c.y ? 2 : 0);
    }

    bool insertInto(Int_Node ni, Vec2 pos, T data, int depth) noexcept {
        if (nodes_[ni].isLeaf()) {
            if (nodes_[ni].item_count < static_cast<u8>(CAPACITY) || depth >= static_cast<int>(MAX_DEPTH))
            {
                Int_Item id = allocItem(pos, std::move(data));
                pushItemToLeaf(ni, id);
                return true;
            }
            const AABB parentBounds = nodes_[ni].bounds;

            const Int_Item oldOffset = nodes_[ni].pool_offset;
            const u8       oldCount  = nodes_[ni].item_count;

            std::array<AABB, 4> cb;
            childAABBs(parentBounds, cb);
            Int_Node c0 = allocNode(cb[0]);
            Int_Node c1 = allocNode(cb[1]);
            Int_Node c2 = allocNode(cb[2]);
            Int_Node c3 = allocNode(cb[3]);

            nodes_[ni].children_id = { c0, c1, c2, c3 };

            nodes_[ni].pool_offset = NULL_ITEM;
            nodes_[ni].item_count  = 0;

            for (u8 i = 0; i < oldCount; ++i) {
                Int_Item iid  = item_pool_[oldOffset + i];
                const Vec2& p = items_[iid].pos;
                int q         = quadrantOf(nodes_[ni].bounds, p);
                pushItemToLeaf(nodes_[ni].children_id[q], iid);
            }

            if (oldOffset + oldCount == static_cast<Int_Item>(item_pool_.size()))
            {
                item_pool_.resize(oldOffset);
            }
        }

        int      q     = quadrantOf(nodes_[ni].bounds, pos);
        Int_Node child = nodes_[ni].children_id[q];
        return insertInto(child, pos, std::move(data), depth + 1);
    }

    template<typename Callback>
    void queryNode(Int_Node ni, const AABB& range, Callback&& cb) const noexcept {
        const NodeCore& n = nodes_[ni];
        if (!n.bounds.intersects(range)) return;

        if (n.isLeaf()) {
            const Int_Item* slot = item_pool_.data() + n.pool_offset;
            for (u8 i = 0; i < n.item_count; ++i) {
                const Item& it = items_[slot[i]];
                if (range.contains(it.pos)) cb(it);
            }
            return;
        }
        for (int i = 0; i < 4; ++i)
            queryNode(n.children_id[i], range, std::forward<Callback>(cb));
    }

    void nearestNode(Int_Node ni, Vec2 t,
                     const Item*& best, Float& bestD2) const noexcept
    {
        const NodeCore& n = nodes_[ni];
        if (n.bounds.minDist2(t) >= bestD2) return;

        if (n.isLeaf()) {
            const Int_Item* slot = item_pool_.data() + n.pool_offset;
            for (u8 i = 0; i < n.item_count; ++i) {
                const Item& it = items_[slot[i]];
                Float dx = it.pos.x - t.x;
                Float dy = it.pos.y - t.y;
                Float d2 = dx * dx + dy * dy;
                if (d2 < bestD2) { bestD2 = d2; best = &it; }
            }
            return;
        }
        const Vec2 c  = n.bounds.center();
        const int  q0 = (t.x >= c.x ? 1 : 0) | (t.y >= c.y ? 2 : 0);
        const int  order[4] = { q0, q0^1, q0^2, q0^3 };
        for (int i : order)
            nearestNode(n.children_id[i], t, best, bestD2);
    }

    template<typename Pred>
    bool removeFrom(Int_Node ni, Pred&& pred) noexcept {
        NodeCore& n = nodes_[ni];

        if (n.isLeaf()) {
            Int_Item* slot = item_pool_.data() + n.pool_offset;
            for (u8 i = 0; i < n.item_count; ++i) {
                if (pred(items_[slot[i]])) {
                    slot[i] = slot[--n.item_count];
                    return true;
                }
            }
            return false;
        }
        for (int i = 0; i < 4; ++i)
            if (removeFrom(n.children_id[i], std::forward<Pred>(pred)))
                return true;
        return false;
    }
};