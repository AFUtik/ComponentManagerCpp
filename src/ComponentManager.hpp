#pragma once

#include <array>
#include <bitset>
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
#include <concepts>
#include <bit>
#include <cstdint>
#include <iostream>

#include "collections/Freelist.hpp"

using u8  = std::uint8_t;
using u16 = std::uint16_t;
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
    using BitsetComp = std::bitset< (MAX_COMPONENTS + 63) & ~size_t(63) >;

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

        template <typename C>
        inline void remove() {
            this->p->template remove_component<C>(*this);   
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
            return this->parent().object_at(this->object_id);
        } 

        inline T& parent() const {
            return *Component<C>::_p;
        }
    };

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

        inline usize size() const {
            return blocks_cnt*BLOCK_SIZE;
        }

        inline u8 block(usize i) {
            return i / BLOCK_SIZE;
        }

        inline BaseComponent* operator[](usize i) const {
            return this->blocks[i / BLOCK_SIZE][i % BLOCK_SIZE];
        }
    };

    template<typename... Components>
    requires (std::derived_from<Components, BaseComponent> && ...)
    struct View {
        View(T *_p) : _p(_p) {}

        struct Iterator {
            Iterator(T* _p, size_t i) : _p(_p), index(i) {skip_invalid();}
            
            inline auto operator*() {
                const Object& e = _p->object_at(index);
                return std::tuple<Components&...>(
                    _p->template get_component<Components>(e)...
                );
            }

            inline void operator++() {
                ++index;
                skip_invalid();
            }

            inline bool operator!=(const Iterator& other) const {
                return index != other.index;
            }
        private:
            T *_p = nullptr;
            u64 index = 0;

            inline void skip_invalid() {
                while (index < _p->objects.size()) {
                    const Object& e = _p->object_at(index);
                    if ((_p->template has_component<Components>(e) && ...)) {
                        break;
                    }
                    ++index;
                }
            }
        };

        Iterator begin() {
            return Iterator(_p,  0);
        }

        Iterator end() {
            return Iterator(_p, _p->objects.size());
        }
    private:
        T *_p = nullptr; 
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
    inline bool has_component(const Object& object) {
        static_assert(std::is_base_of_v<BaseComponent, C>);

        const u64 id = Component<C>::_id;
        auto& array = components[id];

        if(array.blocks_cnt * ComponentArray::BLOCK_SIZE < object.id) return false;
        return array[object.id]->object_id = object.id;
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
        object_components[object.id].set(id);
        
        return *ptr;
    }

    template <typename C>
    inline void remove_component(Object& object) {
        static_assert(std::is_base_of_v<BaseComponent, C>);

        const u64 id = Component<C>::_id;
        auto& array = components[id];
        
        array[object.id]->destroy();
        object_components[object.id].reset(id);
    }

    template <typename C>
    inline C& get_component(const Object& object) const {
        const u64 id = Component<C>::_id; 
        auto& array = components[id];
        return *reinterpret_cast<C*>(array[object.id]);
    }

    inline Object& create_object() {
        u64 i = objects.push(Object(static_cast<T*>(this), invalid));
        object_components.resize(objects.size());

        Object& obj = objects[i];
        obj.id = i;
        return obj;
    }
    
    const Object& object_at(u64 i) const {return objects[i];}

    inline void remove_object(Object& object) {
        BitsetComp& bitset = object_components[object.id];

        auto* data = reinterpret_cast<const uint64_t*>(&bitset);
        constexpr size_t blocks = sizeof(bitset) / 8;

        for (size_t b = 0; b < blocks; ++b) {
            uint64_t v = data[b];
            while (v) {
                int bit = std::countr_zero(v);
                size_t global = b * 64 + bit;

                components[global][object.id]->destroy();

                v &= v - 1;
            }
        }
        bitset.reset();
        object.destroy();
    }

    inline void resize(usize size) {this->size = size;}

    ComponentManager() {
        this->resize(256);
        object_components.reserve(size);
    }
private:
    template <typename C> ComponentArray& get_array() {
        const u64 id = Component<C>::_id;
        return components[id];
    }

    std::array<ComponentArray, MAX_COMPONENTS> components;
    std::array<ComponentType,  MAX_COMPONENTS> components_types;
    u64 components_cnt = 0;

    AllocatedFreelist<Object, I, MAX_OBJECTS> objects;
    std::vector<BitsetComp> object_components;
    u64 size = 0; 
};