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

#include "collections/SparseSet.hpp"

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using usize = u64;

constexpr std::size_t MAX_COMPONENTS = 256;
using ComponentID = std::size_t;

struct Empty {};

struct IObject {
    virtual void init() {};
    virtual void drop() {};
};

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
        Object() = default;
        Object(T *p, I id) : id(id), p(p) {}
        
        template <typename... Args>
        Object(T *p, I id, Args&&... args) : ObjBase(std::forward<Args>(args)...), id(id), p(p) {}
                    
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

        inline void destroy() {
            this->p->remove_object(*this);
        }

        inline bool valid() { return id < invalid;}

        inline I get_id() {return id;}
    private:
        I id = invalid;
        T* p = nullptr;

        friend struct ComponentManager;
    };
    
    using SparseSetObj =  AllocatedSerialSparseSet<Object, I, MAX_OBJECTS>;

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

    struct IComponent {
        virtual void init() {};
        virtual void drop() {};
    };
    
    struct BaseComponent : public IComponent {    
        BaseComponent() = default;

        BaseComponent(const BaseComponent&) = delete;
        BaseComponent& operator=(const BaseComponent&) = delete;

        BaseComponent(BaseComponent&&) noexcept = default;
        BaseComponent& operator=(BaseComponent&&) noexcept = default;

        inline bool valid() {return this->object_id < invalid;}

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
        View(T &_p) : _p(_p) {}

        struct Iterator {
            using arr_iter = std::array<Object, MAX_OBJECTS>::iterator;

            Iterator(T& _p, arr_iter&& obj_iter) : _p(_p), obj_iter(std::move(obj_iter)) {
                skip_invalid();
            }
            
            inline auto operator*() {
                const Object& e = *obj_iter;
                return std::tuple<Components&...>(
                    _p.template get_component<Components>(e)...
                );
            }

            inline void operator++() {
                ++obj_iter;
                skip_invalid();
            }

            inline bool operator!=(const Iterator& other) const {
                return obj_iter != other.obj_iter;
            }

            inline bool operator==(const Iterator& other) const {
                return obj_iter == other.obj_iter;
            }
        private:
            arr_iter obj_iter;
            T &_p;

            inline void skip_invalid() {
                while (obj_iter != _p.objects.end()) 
                {
                    const Object& e = *obj_iter;
                    if ((_p.template has_component<Components>(e) && ...)) {
                        break;
                    }
                    ++obj_iter;
                }
            }
        };

        Iterator begin() {
            return Iterator(_p, _p.objects.begin());
        }

        Iterator end() {
            return Iterator(_p, _p.objects.end());
        }
    private:
        T &_p;
    };

    template <typename V>
    requires (std::derived_from<V, BaseComponent>)
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
        return components_mask[object.id][id];
    }

    template <typename C>
    inline C& add_component(const Object& object, C &&component) {
        static_assert(std::is_base_of_v<BaseComponent, C>);
        
        const u64 id = Component<C>::_id;
        auto& array = components[id];

        array.resize(object.id + 1);

        C* ptr = reinterpret_cast<C*>(array[object.id]);
        new (ptr) C(std::forward<C>(component));

        ptr->object_id = object.id;
        components_mask[object.id].set(id);
        
        ptr->init();
        
        return *ptr;
    }

    template <typename C>
    inline void remove_component(Object& object) {
        static_assert(std::is_base_of_v<BaseComponent, C>);

        const u64 id = Component<C>::_id;
        auto& array = components[id];

        BaseComponent* component = array[object.id];
        component->drop();
        new (component) BaseComponent();

        components_mask[object.id].reset(id);
    }

    template <typename C>
    inline C& get_component(const Object& object) const {
        const u64 id = Component<C>::_id; 
        auto& array = components[id];
        return *reinterpret_cast<C*>(array[object.id]);
    } 
    
    inline const SparseSetObj& get_objects() const {return objects;}
  
    const Object& object_at(u64 i) const {return objects[i];}
    
    template <typename... Args>
    inline Object& create_object(Args&&... args) {
        u64 i = objects.push(Object(static_cast<T*>(this), invalid, std::forward<Args>(args)...));
        components_mask.resize(objects.size());

        Object& obj = objects[i];
        if constexpr (std::is_base_of_v<IObject, ObjBase>) obj.init();
        
        obj.id = i;
        return obj;
    }

    inline void remove_object(Object& object) {
        BitsetComp& bitset = components_mask[object.id];
        iterate_bitset(bitset, [this, id = object.id](size_t component_id) {
            BaseComponent* component = components[component_id][id];
            component->drop();
            new (component) BaseComponent();
        });

        if constexpr (std::is_base_of_v<IObject, ObjBase>) {
            object.drop();
        }

        object = Object();
        bitset.reset();
    }

    inline void resize(usize size) {this->size = size;}

    ComponentManager() {
        this->resize(256);
        components_mask.reserve(size);
    }
private:
    template <typename Fn>
    void iterate_bitset(BitsetComp& bitset, Fn&& fn) {
        auto* data = reinterpret_cast<const uint64_t*>(&bitset);
        constexpr size_t blocks = sizeof(bitset) / 8;

        for (size_t b = 0; b < blocks; ++b) {
            uint64_t v = data[b];
            while (v) {
                int bit = std::countr_zero(v);
                size_t global = b * 64 + bit;

                fn(global);

                v &= v - 1;
            }
        }
    }

    template <typename C> ComponentArray& get_array() {
        const u64 id = Component<C>::_id;
        return components[id];
    }

    std::array<ComponentArray, MAX_COMPONENTS> components;
    std::array<ComponentType,  MAX_COMPONENTS> components_types;
    u64 components_cnt = 0;

    SparseSetObj objects;
    std::vector<BitsetComp> components_mask;
    u64 size = 0; 
};