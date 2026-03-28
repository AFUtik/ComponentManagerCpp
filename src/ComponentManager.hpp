#pragma once

#include <array>
#include <cassert>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <unordered_map>
#include <utility>
#include <array>
#include <new>

#include "Freelist.hpp"

using u8  = std::uint8_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using usize = u64;

constexpr std::size_t MAX_COMPONENTS = 256;
using ComponentID = std::size_t;

struct Empty {};

template <
    typename T,
    typename I,
    typename ObjBase,
    u64 MAX_COMPONENTS,
    u64 MAX_OBJECTS>
struct ComponentManager {
    static constexpr I invalid = std::numeric_limits<I>::max();

    struct Object : public ObjBase {
        I id = invalid;
        
        Object() = default;
        Object(T *p, I id) : id(id), p(p) {}
                    
        template <typename C>
        inline C& get() const {
            return this->p->template get_component<C>(*this);
        }

        template <typename C>
        inline void add(C&& component) {
            this->p->template add_component<C>(*this, std::forward<C>(component));   
        }

        inline bool is_valid() { return id < invalid;}

        inline void destroy() {
            id = invalid;
            p  = nullptr;
        }
    private:
        T* p = nullptr;
    };

    struct ComponentType {
        u64 id;
        usize size;

        template<typename C>
        constexpr static inline ComponentType from(u64 id) {
            ComponentType result;
            result.id = id;
            result.size = sizeof(C) + alignof(C);
            return result;
        }
    };
    
    struct BaseComponent {    
        BaseComponent() = default;

        BaseComponent(const BaseComponent&) = delete;
        BaseComponent& operator=(const BaseComponent&) = delete;

        BaseComponent(BaseComponent&&) noexcept = default;
        BaseComponent& operator=(BaseComponent&&) noexcept = default;

        virtual void init() {};
        
        inline bool is_valid() {
            return this->object_id < invalid;
        }

        inline void destroy() {
            object_id = invalid;
        }

        I object_id = invalid;
    };
 
    template <typename C> 
    struct Component : public BaseComponent {
        inline static u64 _id;
        inline static T *_p;

        inline const Object& object() const {
            return this->parent().object_by_index(this->object_id);
        } 

        inline T& parent() const {
            return *Component<C>::_p;
        }
    };

    // Components //
    struct ComponentArray {
        static constexpr auto BLOCK_SIZE = 512;
        static constexpr auto NUM_BLOCKS = (MAX_OBJECTS / BLOCK_SIZE) + 1;

        struct AlignedArrayDeleter {
            void operator()(u8* ptr) const noexcept {
                ::operator delete[](ptr, std::align_val_t(16));
            }
        };

        struct Block {
            const ComponentArray *parent = nullptr;
            std::unique_ptr<u8[], AlignedArrayDeleter> data = nullptr;

            Block() = default;

            Block(const ComponentArray *parent, usize n) : parent(parent) {
                const auto data_size = BLOCK_SIZE * this->parent->type->size;

                this->data = std::unique_ptr<u8[], AlignedArrayDeleter>(new (std::align_val_t(16)) u8[data_size]);

                std::memset(data.get(), 0, data_size);
            }

            inline BaseComponent* operator[](usize i) const {
                assert(i < BLOCK_SIZE);
                return reinterpret_cast<BaseComponent*>(reinterpret_cast<u8*>(this->data.get()) + (this->parent->type->size * i));
            }
        };

        ComponentType* type = nullptr;

        Block blocks[NUM_BLOCKS];
        usize blocks_cnt = 0;

        ComponentArray() = default;
        ComponentArray(ComponentType *type) : type(type) {}

        void resize(usize n) {
            while(n > (this->blocks_cnt * BLOCK_SIZE)) {
                assert(this->blocks_cnt != NUM_BLOCKS);
                this->blocks[this->blocks_cnt] = Block(this, this->blocks_cnt);
                this->blocks_cnt++;
            }
        }

        inline u8 block(usize i) {
            return i / BLOCK_SIZE;
        }

        inline BaseComponent* operator[](usize i) const {
            return this->blocks[i / BLOCK_SIZE][i % BLOCK_SIZE];
        }
    };

    template <typename V>
    void register_type() {
        const u64 id = this->components_cnt++;
        assert(this->components_cnt <= MAX_COMPONENTS);
     
        Component<V>::_id = id;
        V::_p = static_cast<T*>(this);
        this->components_types[id] = ComponentType::template from<V>(id);
        this->components[id] = ComponentArray(&this->components_types[id]);
        this->components[id].resize(this->size);
    }
    
    template <typename C>
    inline bool has_component(Object& object) {
        static_assert(std::is_base_of_v<BaseComponent, C>);

        const u64 id = Component<C>::_id;
        auto& array = components[id];

        if(array.blocks_cnt * ComponentArray::BLOCK_SIZE < object.id) return false;
        return array[object.id].is_valid();
    }

    template <typename C>
    inline C& add_component(Object& object, C &&component) {
        static_assert(std::is_base_of_v<BaseComponent, C>);
        
        const u64 id = Component<C>::_id;
        auto& array = components[id];

        array.resize(object.id + 1);

        C* ptr = reinterpret_cast<C*>(array[object.id]);
        new (ptr) C(std::move(component));

        ptr->object_id = object.id;
        
        return *ptr;
    }

    template <typename C>
    inline void remove_component(Object& object) {
        static_assert(std::is_base_of_v<BaseComponent, C>);

        const u64 id = Component<C>::_id;
        auto& array = components[id];
        
        array[object.id]->destroy(); 
    }

    template <typename C>
    inline C& get_component(const Object& object) const {
        const u64 id = Component<C>::_id; 
        auto& array = components[id];
        return *reinterpret_cast<C*>(array[object.id]);
    }

    inline Object& create_object() {
        u64 i = objects.push(Object(static_cast<T*>(this), invalid));

        Object& obj = objects[i];
        obj.id = i;
        return obj;
    }
    
    const Object& object_by_index(u64 i) const {
        return objects[i];
    }

    inline void remove_object(Object& object) {
        auto it = object_components.find(object.id);
        if(it != object_components.end()) {
            for(u64 i : it.second) {
                auto& array = components[i];
                array[object.id]->destroy();
            }
        }
        object_components.erase(it);
        objects.erase(object.id);
    }

    inline void resize(usize size) {this->size = size;}

    ComponentManager() {this->resize(256);}
private:
    std::array<ComponentArray, MAX_COMPONENTS> components;
    std::array<ComponentType,  MAX_COMPONENTS> components_types;
    u64 components_cnt = 0;

    AllocatedFreelist<Object, I, MAX_OBJECTS> objects;
    u64 free_head = 0;

    std::unordered_map<u64, std::vector<u32>> object_components;    
    u64 size = 0; 
};