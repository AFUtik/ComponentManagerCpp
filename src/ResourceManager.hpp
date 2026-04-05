#pragma once

#include <concepts>
#include <limits>
#include <vector>
#include <unordered_map>
#include <functional>

#include "collections/Freelist.hpp"
#include "collections/SparseSet.hpp"

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using usize = u64;

template<typename T, typename MapKey, typename... Args>
class ResourceFactory {
public:
    using CreateFn = std::function<T(Args...)>;

    template<typename F>
    void register_factory(F&& fn) {
        creator = std::forward<F>(fn);
    }

    template<typename... CallArgs>
    T create(CallArgs&&... args) const {
        return creator(std::forward<CallArgs>(args)...);
    }

    static ResourceFactory& instance() {
        static ResourceFactory factory;
        return factory;
    }
private:
    CreateFn creator;
};

template<
    typename T, 
    typename MapKey  = std::string
>
struct ResourceManager {
    static constexpr u32 invalid = std::numeric_limits<u32>::max();

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
            if(id!=invalid) ResourceManager::instance().decrement_reference(id);
        }

        ReferencedResource(ReferencedResource&& other) noexcept {
            move_from(other);
        }

        ReferencedResource& operator=(ReferencedResource&& other) noexcept {
            if (this != &other) {
                if(id!=invalid) ResourceManager::instance().decrement_reference(id);
                move_from(other);
            }
            return *this;
        }

        ReferencedResource(const ReferencedResource& other) : id(other.id)
        {
            if(id!=invalid) ResourceManager::instance().increment_reference(id);
        }
        
        ReferencedResource& operator=(const ReferencedResource& other) {
            if (this != &other) {
                if(id!=invalid) ResourceManager::instance().decrement_reference(id);

                id = other.id;

                if(id!=invalid) ResourceManager::instance().increment_reference(id);
            }
            return *this;
        }

        explicit operator bool() const {return id != invalid;}

        // id can point to other object after destruction of ManagedResource. //
        inline u32 get_id_nosafe() const {return id;}
    private:
        ReferencedResource(u32 id) : id(id) {
            ResourceManager::instance().increment_reference(id);
        }

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
    requires (std::same_as<K, ReferencedResource> || std::same_as<K, ManagedResource>)
    inline K create(U&& resource) {
        const u32 id = resources.push(std::forward<U>(resource));
        
        if constexpr(std::is_same_v<K, ReferencedResource>) references.resize(id+1);
        
        return K(id);
    }

    template<typename... Args>
    ReferencedResource load(const MapKey& key, Args&&... args) {
        if (auto it = resources_map.find(key); it != resources_map.end() && !expired(it->second)) {
            return ReferencedResource(it->second);
        }

        auto& factory = ResourceFactory<T, MapKey, Args...>::instance();
        T value = factory.create(std::forward<Args>(args)...);

        auto ref = create<ReferencedResource>(std::move(value));
        resources_map[key] = ref.get_id_nosafe();
        return ref;
    }

    inline bool expired(u32 id) {
        return references[id].counter == 0 || references[id].id != id; 
    }
private:
    struct Reference {
        u32 counter = 0;
        u32 id = invalid;
    };

    SerialSparseSet<T, u32> resources;
    std::vector<Reference> references; // counter, resource_id //
    T default_resource;

    std::unordered_map<MapKey, u32> resources_map;

    //std::vector<ResourceFactory>    factories;

    inline void increment_reference(u32 id) 
    {
        references[id].counter++;
    }

    inline void decrement_reference(u32 id) 
    {
        if (--references[id].counter == 0)
        {
            resources.erase(id);
            references[id] = Reference();
        }
    }
    
    inline void release(u32 id) {resources.erase(id);}
};

template<typename T, typename MapKey = std::string>
class Resource {
public:
    using Manager = ResourceManager<T, MapKey>;
    using Ref = typename Manager::ReferencedResource;

    Resource() = default;

    static Resource load(const MapKey& key) {
        return Resource(Manager::instance().load(key));
    }

    template<typename... Args>
    static Resource load(const MapKey& key, Args&&... args) {
        return Resource(Manager::instance().load(key, std::forward<Args>(args)...));
    }

    bool valid() const {
        return static_cast<bool>(ref_);
    }

    explicit operator bool() const {
        return valid();
    }

    T& get() {
        return Manager::instance().get_data(ref_);
    }

    const T& get() const {
        return Manager::instance().get_data(ref_);
    }

    T* operator->() {
        return &get();
    }

    const T* operator->() const {
        return &get();
    }

    T& operator*() {
        return get();
    }

    const T& operator*() const {
        return get();
    }

private:
    explicit Resource(Ref ref) : ref_(std::move(ref)) {}

    Ref ref_;
};