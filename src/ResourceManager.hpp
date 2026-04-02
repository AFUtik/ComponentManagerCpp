#pragma once

#include <vector>
#include <unordered_map>

#include "collections/SparseSet.hpp"

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using usize = u64;

template<
    typename T, 
    typename I = u64
>
struct ResourceManager {
    static constexpr I invalid = std::numeric_limits<I>::max();

    struct ManagedResource {
        ManagedResource() {};

        ~ManagedResource() 
        {
            ResourceManager::instance().delete_managed_resource(*this);
        }
        
        ManagedResource(const ManagedResource&) = delete;
        ManagedResource& operator=(const ManagedResource&) = delete;

        ManagedResource(ManagedResource&& other) noexcept : id(other.id) {
            other.id = invalid;
        }

        ManagedResource& operator=(ManagedResource&& other) noexcept {
            id = other.id;
            other.id = invalid;
        }
    private:
        ManagedResource(I id) : id(id) {};

        I id = invalid;

        friend class ResourceManager;
    };

    struct Resource {
        Resource(I id) : id(id) {}

        Resource() = default;

        I id = invalid;
    };

    static ResourceManager& instance() {
        static ResourceManager manager;
        return manager;   
    }

    inline Resource get(const std::string& key) {
        assert(!key.empty());

        auto it = resource_map.find(key);

        assert(it!=resource_map.end());
        assert(it.second!=invalid && "The resource was allocated");

        return Resource(it.second);
    }

    inline void rename(const std::string& key, const std::string& new_key) {
        assert(!key.empty());
        
        auto it = resource_map.find(key);

        assert(it!=resource_map.end());
        assert(it.second!=invalid && "The resource was allocated");

        I id = it.second;

        resource_map.erase(it);
        resource_map.emplace(new_key, id);

        keys[id] = new_key;
    }

    inline const SerialSparseSet<T, I>& get_resources() 
    {
        return resource_set;
    }
private:
    std::unordered_map<std::string, I> resource_map;
    std::vector<std::string> keys;

    SerialSparseSet<T, I> resource_set;
    
    inline void delete_managed_resource(const ManagedResource& resource) {
        assert(resource.id!=invalid);

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
    inline ManagedResource create_managed_resource(U&& resource) {
        I id = resource_set.push(std::forward<U>(resource));
        return ManagedResource(id);
    }

    template <typename U>
    inline ManagedResource create_managed_resource(U&& resource, const std::string& key) {
        I id = resource_set.push(std::forward<U>(resource));

        resource_map.emplace(key, id);

        keys.resize(static_cast<u64>(id));
        keys[id] = key;

        return ManagedResource(id);
    }
};