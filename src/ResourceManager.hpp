#pragma once

#include <concepts>
#include <limits>
#include <vector>
#include <unordered_map>

#include "collections/Freelist.hpp"
#include "collections/SparseSet.hpp"

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using usize = u64;

static constexpr u32 invalid = std::numeric_limits<u32>::max();

struct __ReferenceController
{   
    static inline __ReferenceController& instance() 
    {
        static __ReferenceController controller;
        return controller;
    }
private:    
    Freelist<u32, u32> ref_counters;
    std::vector<u32> ids;

    inline u32 create_reference(u32 resource_id)
    {
        const u32 id = ref_counters.push(0);
        ids.resize(id+1, invalid);
        ids[id] = resource_id; 
        return id;
    }

    inline void increment_reference(u32 ref_id)
    {
        ref_counters[ref_id]++; 
    }

    template <typename T>
    inline void decrement_reference(u32 resource_id, u32 ref_id) 
    {
        if(--ref_counters[ref_id] == 0) {
            T::instance().release(resource_id); 
            ids[ref_id] = invalid;
        }
    }

    inline bool expired(u32 resource_id, u32 ref_id) 
    {
        return ref_counters[resource_id] == 0 || ids[ref_id] != resource_id;
    }

    template<typename, typename>
    friend class ResourceManager;
};

template<typename T, typename MapKey = std::string>
struct ResourceManager {
    using CreateFn = T(*)();

    struct ManagedResource {
        ManagedResource() = default;

        ~ManagedResource()
        {
            if(id!=invalid) ResourceManager::instance().release(id);
        }
        
        ManagedResource(const ManagedResource&) = delete;
        ManagedResource& operator=(const ManagedResource&) = delete;

        ManagedResource(ManagedResource&& other) noexcept : id(other.id) {
            other.id = invalid;
        }

        ManagedResource& operator=(ManagedResource&& other) noexcept {
            if (this != &other) {
                if(id!=invalid) ResourceManager::instance().release(id);

                id = other.id;
                other.id = invalid;
            }
            return *this;
        }

        inline void release() {
            if(id!=invalid) {
                ResourceManager::instance().release(id);
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
            if(id!=invalid) __ReferenceController::instance().decrement_reference<ResourceManager>(id, ref_id);
        }

        ReferencedResource(ReferencedResource&& other) noexcept {
            move_from(other);
        }

        ReferencedResource& operator=(ReferencedResource&& other) noexcept {
            if (this != &other) {
                if(id!=invalid) __ReferenceController::instance().decrement_reference<ResourceManager>(id, ref_id);
                move_from(other);
            }
            return *this;
        }

        ReferencedResource(const ReferencedResource& other) : id(other.id)
        {
            if(id!=invalid) __ReferenceController::instance().increment_reference(ref_id);
        }
        
        ReferencedResource& operator=(const ReferencedResource& other) {
            if (this != &other) {
                if(id!=invalid) __ReferenceController::instance().decrement_reference<ResourceManager>(id, ref_id);

                id = other.id; ref_id = other.ref_id;

                if(id!=invalid) __ReferenceController::instance().increment_reference(ref_id);
            }
            return *this;
        }

        explicit operator bool() const {return id != invalid && ref_id != invalid;}

        // id can point to other object after destruction of ManagedResource. //
        inline u32 get_id_nosafe() const {return id;}
    private:
        ReferencedResource(u32 id) : id(id), ref_id(__ReferenceController::instance().create_reference(id)) {}

        inline void move_from(ReferencedResource& other) {
            id = other.id;
            ref_id = other.ref_id;

            other.id = invalid;
            other.ref_id = invalid;
        }

        u32 id = invalid;
        u32 ref_id = invalid;

        friend class ResourceManager;
    };

    struct WeakResource {
        inline bool expired() const {
            return id != invalid && ref_id != invalid ? __ReferenceController::instance().expired(id, ref_id) : true;
        }

        inline ReferencedResource ref() {
            return expired() ? ReferencedResource() : ReferencedResource(id, ref_id);
        }

        WeakResource() {}

        WeakResource(const ReferencedResource& other) : id(other.id), ref_id(other.ref_id) {}
        
        WeakResource& operator=(const ReferencedResource& other) {
            id = other.id;
            ref_id = other.ref_id;
        }
    private:
        u32 id = invalid;
        u32 ref_id = invalid;
    };
    
    struct ResourceFactory {
        CreateFn create_fn = nullptr;
    private:
        WeakResource weak_resource;
    };

    static ResourceManager& instance() {
        static ResourceManager manager;
        return manager;   
    }

    inline T& get(u32 id) 
    {
        return resources[id];
    }

    inline T& get_data(const ManagedResource& resource) 
    {
        return resources[resource.id];
    }

    inline T& get_data(const ReferencedResource& resource) 
    {
        return resources[resource.id];
    }

    inline const SerialSparseSet<T, u32>& get_resources() 
    {
        return resources;
    }
    
    template <typename K, typename U>
    requires (std::derived_from<K, ReferencedResource> || std::derived_from<K, ManagedResource>)
    inline K create(U&& resource) {
        return K(resources.push(std::forward<U>(resource)));
    }
private:
    SerialSparseSet<T, u32> resources;
    T default_resource;

    std::unordered_map<MapKey, u32> factory_map;
    std::vector<ResourceFactory>    factories;

    inline void release(u32 resource_id) {resources.erase(resource_id);}
};

template <typename T, typename MapKey>
struct Resource
{
    using Manager = ResourceManager<T, MapKey>;
    const Manager& rm = Manager::instance();

private:
    Resource(const MapKey& key) {
        
    }

    Manager::ReferencedResource resource_ref;
    u32 factory_id = invalid;
};