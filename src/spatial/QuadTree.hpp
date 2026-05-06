#pragma once

#include "Spatial.hpp"

#include <vector>
#include <span>
#include <limits>
#include <cassert>
#include <iostream>
#include <vector>
#include <memory>

template<
    typename T,
    typename V,
    typename Float = float,
    typename Vec2  = Vec2Base<Float>>
struct QuadTree
{
    using AABB = AABB2D<Float, Vec2>;

    static constexpr u32 NULL_NODE  = std::numeric_limits<u32>::max();
    static constexpr u32 NO_STORAGE = std::numeric_limits<u32>::max();
 
    static constexpr u32 HEAP_FLAG  = 0x8000'0000u;

    static constexpr bool IS_POINT = std::is_same_v<V, Vec2>;
 
    struct Node
    {
        AABB  bounds;
        u32   firstChild  = NULL_NODE;  
        u32   itemStorage = NO_STORAGE; 
        u16   itemCount   = 0;
        u8    depth       = 0;
 
        bool isLeaf()    const noexcept { return firstChild == NULL_NODE; }
        bool usesHeap()  const noexcept { return (itemStorage & HEAP_FLAG) != 0; }
        u32  poolOffset()const noexcept { return itemStorage; }              
        u32  heapIndex() const noexcept { return itemStorage & ~HEAP_FLAG; }
    };

    struct Item
    {
        T id;
        V primitive;
    };

    void init(
        AABB rootBounds,
        u32  maxDepth_        = 6,
        u32  nodeCapacity_    = 8,
        u32  reserveNodes     = 64,
        u32  reserveItems     = 256,
        u32  reserveHeapBkts  = 8
    ) {
        maxDepth     = maxDepth_;
        nodeCapacity = nodeCapacity_;
 
        nodes_.clear();
        nodes_.reserve(reserveNodes);
 
        pool_.clear();
        pool_.reserve(reserveItems);
 
        heap_.clear();
        heap_.reserve(reserveHeapBkts);
 
        Node root;
        root.bounds = rootBounds;
        nodes_.push_back(root);
    }
 
    void clear() noexcept
    {
        AABB rootBounds = nodes_[0].bounds; 
        nodes_.resize(1);
        nodes_[0]        = Node{};
        nodes_[0].bounds = rootBounds;
        pool_.clear();
        heap_.clear();
    }
 
    void reset(AABB worldBounds) noexcept
    {
        nodes_.resize(1);
        nodes_[0]        = Node{};
        nodes_[0].bounds = worldBounds;
        pool_.clear();
        heap_.clear();
    }
 
    bool insert(T data, V primitive) noexcept
    {
        if constexpr(IS_POINT) 
            if (!nodes_[0].bounds.contains(primitive)) 
                return false;
        
        insertInto(0, Item{data, primitive});
        return true;
    }
    
    /*
    template<typename Callback>
    void query(const AABB& range, Callback&& cb) const noexcept
    {
        queryNode(0, range, std::forward<Callback>(cb));
    }
    */
 
    u32 nodeCount() const noexcept { return static_cast<u32>(nodes_.size()); }
    std::span<const Node> nodes() const noexcept { return nodes_; }
 
    u32 maxDepth     = 6;
    u32 nodeCapacity = 8;
private:
    std::vector<Node>              nodes_;
    std::vector<Item>              pool_;
    std::vector<std::vector<Item>> heap_;
 
    u32 allocChildren()
    {
        const u32 base = static_cast<u32>(nodes_.size());
        nodes_.resize(base + 4);
        return base;
    }
 
    u32 allocPoolSlot()
    {
        const u32 off = static_cast<u32>(pool_.size());
        pool_.resize(off + nodeCapacity);
        return off;
    }
 
    void poolPush(u32 ni, const Item& item)
    {
        Node& n = nodes_[ni];

        if (n.itemStorage == NO_STORAGE)
            n.itemStorage = allocPoolSlot();

        u32 offset = n.poolOffset();

        assert(offset != NO_STORAGE);
        assert(n.itemCount < nodeCapacity);

        pool_[offset + n.itemCount] = item;
        ++n.itemCount;
    }
 
    void promoteToHeap(u32 ni)
    {
        assert(nodes_[ni].isLeaf());
        assert(!nodes_[ni].usesHeap());
 
        const u32 hIdx = static_cast<u32>(heap_.size());
        heap_.emplace_back();
        auto& bucket = heap_.back();
        bucket.reserve(nodeCapacity * 2);
 
        if (nodes_[ni].itemStorage != NO_STORAGE) {
            const u32 off = nodes_[ni].poolOffset();
            for (u16 i = 0; i < nodes_[ni].itemCount; ++i) bucket.push_back(pool_[off + i]);
        }
        nodes_[ni].itemStorage = hIdx | HEAP_FLAG;
    }
 
    void insertInto(u32 ni, const Item& item) noexcept
    {
        while (true)
        {
            const bool isLeaf = nodes_[ni].isLeaf();
            const bool maxed  = nodes_[ni].depth >= static_cast<u8>(maxDepth);
            const bool canFit = nodes_[ni].itemCount < static_cast<u16>(nodeCapacity);
 
            if (isLeaf)
            {
                if (canFit)
                {
                    poolPush(ni, item);
                    return;
                }
 
                if (maxed)
                {
                    if (!nodes_[ni].usesHeap())
                        promoteToHeap(ni);
 
                    heap_[nodes_[ni].heapIndex()].push_back(item);
                    ++nodes_[ni].itemCount;
                    return;
                }
 
                splitNode(ni);
            }

            if constexpr(IS_POINT)
            {
                const Vec2 c = nodes_[ni].bounds.center();
                const int  q = (item.primitive.x >= c.x ? 1 : 0) | (item.primitive.y >= c.y ? 2 : 0);
                ni = nodes_[ni].firstChild + static_cast<u32>(q);
            }
        }
    }
 
    void splitNode(u32 ni) noexcept
    {
        const AABB pb        = nodes_[ni].bounds;
        const u32  oldStore  = nodes_[ni].itemStorage;
        const u16  oldCount  = nodes_[ni].itemCount;
        const u8   childDepth= static_cast<u8>(nodes_[ni].depth + 1);

        const u32 base = allocChildren();
 
        const Vec2 c = pb.center();
        const AABB quads[4] = {
            { pb.minX, pb.minY, c.x,     c.y     }, // SW
            { c.x,     pb.minY, pb.maxX, c.y     }, // SE
            { pb.minX, c.y,     c.x,     pb.maxY }, // NW
            { c.x,     c.y,     pb.maxX, pb.maxY }, // NE
        };

        for (int i = 0; i < 4; ++i) {
            nodes_[base + i]        = Node{};
            nodes_[base + i].bounds = quads[i];
            nodes_[base + i].depth  = childDepth;
        }

        nodes_[ni].firstChild  = base;
        nodes_[ni].itemStorage = NO_STORAGE;
        nodes_[ni].itemCount   = 0;
 
        for (int i = 0; i < oldCount; i++) {
            const Item& item = pool_[oldStore + i];

            const Vec2 cc = nodes_[ni].bounds.center();
            const int  q  = (item.primitive.x >= cc.x ? 1 : 0) | (item.primitive.y >= cc.y ? 2 : 0);
            insertInto(base + static_cast<u32>(q), item);
        }
        nodes_[base].itemStorage = oldStore;
    }
    
    /*
    template<typename Callback>
    void queryNode(u32 ni, const AABB& range, Callback&& cb) const noexcept
    {
        const Node& node = nodes_[ni];
        if (!node.bounds.overlaps(range)) return;
 
        if (node.isLeaf())
        {
            if (node.itemStorage == NO_STORAGE) return;
 
            auto visit = [&](const T& item) {
                if constexpr (DiversePrimitives) {
                    if (overlaps(item, range)) cb(item);
                } else {
                    if (range.contains(getPos(item))) cb(item);
                }
            };
 
            if (!node.usesHeap()) {
                const u32 off = node.poolOffset();
                for (u16 i = 0; i < node.itemCount; ++i)
                    visit(pool_[off + i]);
            } else {
                for (const T& item : heap_[node.heapIndex()])
                    visit(item);
            }
            return;
        }
 
        // Внутренний узел: проверяем все 4 квадранта
        for (u32 i = 0; i < 4; ++i)
            queryNode(node.firstChild + i, range, std::forward<Callback>(cb));
    }
        */
};