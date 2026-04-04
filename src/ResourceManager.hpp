#pragma once

#include <concepts>
#include <limits>
#include <vector>
#include <unordered_map>

#include "collections/SparseSet.hpp"

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using usize = u64;

template<typename T>
struct GlobalResource
{
private:
    GlobalResource(u32 id) : id(id) {}

    u32 id = std::numeric_limits<u32>::max();

    template<typename, typename>
    friend struct ResourceManager;
};

template<typename T, typename MapKey = std::string>
struct ResourceManager {
    static constexpr u32 invalid = std::numeric_limits<u32>::max();
    
    using CreateFn = T(*)();
    
    struct ManagedResource {
        ManagedResource() = default;

        ~ManagedResource()
        {
            if(id!=invalid) ResourceManager::instance().release_managed(*this);
        }
        
        ManagedResource(const ManagedResource&) = delete;
        ManagedResource& operator=(const ManagedResource&) = delete;

        ManagedResource(ManagedResource&& other) noexcept : id(other.id) {
            other.id = invalid;
        }

        ManagedResource& operator=(ManagedResource&& other) noexcept {
            if (this != &other) {
                if(id!=invalid) ResourceManager::instance().release_managed(*this);

                id = other.id;
                other.id = invalid;
            }
            return *this;
        }

        inline void release() {
            if(id!=invalid) {
                ResourceManager::instance().release_managed(*this);
                id=invalid;
            }
        }

        // id can point to other object after destruction of ReferencedResource. //
        inline u32 get_id_nosafe() const {return id;}
    private:
        ManagedResource(u32 id) : id(id) {}

        u32 id = invalid;

        friend struct ResourceManager;
    };

    struct ReferencedResource {
        ReferencedResource() = default;

        ~ReferencedResource() {
            if(id!=invalid) ResourceManager::instance().release_referenced(*this);
        }

        ReferencedResource(ReferencedResource&& other) noexcept {
            move_from(other);
        }

        ReferencedResource& operator=(ReferencedResource&& other) noexcept {
            if (this != &other) {
                if(id!=invalid) ResourceManager::instance().release_referenced(*this);
                move_from(other);
            }
            return *this;
        }

        ReferencedResource(const ReferencedResource& other) : id(other.id)
        {
            if(id!=invalid) ResourceManager::instance().increment_referenced(*this);
        }
        
        ReferencedResource& operator=(const ReferencedResource& other) {
            if (this != &other) {
                if(id!=invalid) ResourceManager::instance().release_referenced(*this);

                id = other.id;

                if(id!=invalid) ResourceManager::instance().increment_referenced(*this);
            }
            return *this;
        }

        inline bool valid() {return id != invalid;}

        // id can point to other object after destruction of ManagedResource. //
        inline u32 get_id_nosafe() const {return id;}
    private:
        ReferencedResource(u32 id) : id(id) {}

        inline void move_from(ReferencedResource& other) {
            id = other.id;
            other.id = invalid;
        }

        u32 id = invalid;

        friend class ResourceManager;
    };  
    
    static ResourceManager& instance() {
        static ResourceManager manager;
        return manager;   
    }
    
    inline T& get_data(GlobalResource<T> resource) 
    {
        return resource_set[resource.id];
    }

    inline T& get_data(const ManagedResource& resource) 
    {
        return resource_set[resource.id];
    }

    inline T& get_data(const ReferencedResource& resource) 
    {
        return resource_set[resource.id];
    }

    inline const SerialSparseSet<T, u32>& get_resources() 
    {
        return resource_set;
    }

    template <typename U>
    inline const GlobalResource<T> create(const MapKey &key, U&& resource) {
        const u32 id = resource_set.push(std::forward<U>(resource));
        resource_map.emplace(key, id);
        return GlobalResource<T>(id);
    }
    
    inline const GlobalResource<T> get(const MapKey& key) 
    {
        return GlobalResource<T>(resource_map.at(key));
    }
    
    template <typename U>
    inline ManagedResource create_managed(U&& resource) {
        const u32 id = resource_set.push(std::forward<U>(resource));
        return ManagedResource(id);
    }

    template <typename U>
    inline ReferencedResource create_referenced(U&& resource) {
        const u32 id = resource_set.push(std::forward<U>(resource));

        if (id >= references_count.size()) references_count.resize(id + 1, 0);
    
        return ReferencedResource(id);
    }
private:
    SerialSparseSet<T, u32> resource_set;
    std::vector<u32> references_count;
    std::unordered_map<MapKey, u32> resource_map;

    // REFERENCED OP //
    inline bool is_valid_reference(u32 id) 
    {
        assert(id < references_count.size());

        return references_count[id] > 0;
    }

    inline void increment_referenced(const ReferencedResource& resource) 
    {
        assert(resource.id != invalid);

        references_count[resource.id]++;
    }

    inline void release_referenced(ReferencedResource& resource) 
    {
        const u32 id = resource.id;
        assert(id != invalid);
        
        if (--references_count[id] == 0)
        {
            resource_set.erase(id);
            references_count[id] = 0;
        }
    }
    
    // MANAGED OP //
    inline void release_managed(const ManagedResource& resource) {
        assert(resource.id != invalid);

        resource_set.erase(resource.id);
    }
};


