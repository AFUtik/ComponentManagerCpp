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
class ResourceManager;


template<typename T>
struct ResourceManager {
    static constexpr u32 invalid = std::numeric_limits<u32>::max();
    
    using CreateFn = T(*)();
    
    struct ConstResource {
        ConstResource() = default;
    private:
        ConstResource(u32 id) : id(id) {}

        u32 id = invalid;

        friend struct ResourceManager;

        friend struct ManagedResource;
        friend struct ReferencedResource;
    };

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

        inline bool valid() {return id != invalid && ResourceManager::instance().get_ref_count(*this) != 0;}
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

    inline ConstResource get(const std::string& key) {
        assert(!key.empty());

        auto it = resource_map.find(key);

        assert(it!=resource_map.end());
        assert(it->second!=invalid && "The resource was allocated");

        return Resource(it->second);
    }

    inline void rename(const std::string& key, const std::string& new_key) {
        assert(!key.empty());
        
        auto it = resource_map.find(key);

        assert(it!=resource_map.end());
        assert(it->second!=invalid && "The resource was allocated");

        u32 id = it->second;

        resource_map.erase(it);
        resource_map.emplace(new_key, id);

        keys[id] = new_key;
    }

    inline const SerialSparseSet<T, u32>& get_resources() 
    {
        return resource_set;
    }
private:
    std::unordered_map<std::string, u32> resource_map;
    
    SerialSparseSet<T, u32> resource_set;
    std::vector<CreateFn>   resource_factory;

    std::vector<u32> references_count;
    std::vector<std::string> keys;

    // REFERENCED OP //
    inline u32 get_ref_count(const ReferencedResource& resource) 
    {
        assert(resource.id < references_count.size());

        return references_count[resource.id];
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

        if(resource.id >= keys.size()) return;
        
        std::string &key = keys[resource.id];
        if(!key.empty()) {
            auto it = resource_map.find(key);
            if(it != resource_map.end()) resource_map.erase(it);
            
            key.clear();
        }
    }
protected:
    template <typename U>
    inline ManagedResource create_managed(U&& resource) {
        const u32 id = resource_set.push(std::forward<U>(resource));
        return ManagedResource(id);
    }

    template <typename U>
    inline ReferencedResource create_referenced(U&& resource) {
        const u32 id = resource_set.push(std::forward<U>(resource));

        if (id >= references_count.size()) references_count.resize(id + 1, 0);
    
        return ReferencedResource(id, &references_count[id]);
    }

    template <typename U>
    inline ManagedResource create_managed(U&& resource, const std::string& key) {
        const u32 id = resource_set.push(std::forward<U>(resource));

        resource_map.emplace(key, id);

        if(id > keys.size()) keys.resize(static_cast<u64>(id));
        keys[id] = key;

        return ManagedResource(id);
    }
};


