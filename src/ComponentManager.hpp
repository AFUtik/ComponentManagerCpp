#pragma once

#include <array>
#include <bitset>
#include <cassert>
#include <cstdint>
#include <limits>
#include <memory>
#include <type_traits>
#include <utility>
#include <array>
#include <new>
#include <concepts>
#include <bit>
#include <cstdint>

#include "collections/SparseSet.hpp"

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using usize = u64;

struct Empty {};

struct IObject {
    virtual void init() {};
    virtual void drop() {};
};

template <
    typename T,
    typename I,
    typename ObjBase,
    size_t MAX_COMPONENTS>
struct ComponentManager {
    using BitsetComp = std::bitset< (MAX_COMPONENTS + 63) & ~size_t(63) >;

    static constexpr I invalid = std::numeric_limits<I>::max();
    
    struct Object : public ObjBase {
        Object() = default;
        Object(T *p, I id) : id(id), p(p) {}
        
        template <typename... Args>
        Object(T *p, I id, Args&&... args) : ObjBase(std::forward<Args>(args)...), id(id), p(p) {}
                    
        template <typename C>
        C& get() const {
            return this->p->template get_component<C>(*this);
        }

        template <typename C>
        void add(C&& component) {
            this->p->template add_component<C>(*this, std::forward<C>(component));   
        }

        template <typename C>
        void remove() {
            this->p->template remove_component<C>(*this);   
        }

        void destroy() {
            this->p->remove_object(*this);
        }

        bool valid() { return id < invalid;}

        I get_id() {return id;}
    private:
        I id = invalid;
        T* p = nullptr;

        friend struct ComponentManager;
    };
    
    struct ComponentType {
        u64 id;
        usize size;
        usize align;
        void (*destroy)(void*) = nullptr;

        template<typename C>
        static ComponentType from(u64 id) {
            return {
                id,
                sizeof(C),
                alignof(C),
                [](void* p) { std::destroy_at(static_cast<C*>(p)); }
            };
        }
    };

    struct BaseComponent {    
        BaseComponent() = default;

        BaseComponent(const BaseComponent&) = delete;
        BaseComponent& operator=(const BaseComponent&) = delete;

        BaseComponent(BaseComponent&&) noexcept = default;
        BaseComponent& operator=(BaseComponent&&) noexcept = default;
    };
 
    template <typename C> 
    struct Component : public BaseComponent {
        inline static u64 _id;
        inline static T *_p;
        u8 block = 0;

        const Object& object() const {
            return _p->object_at( _p->template get_array<C>().get_index(block, this) );
        } 

        T& parent() const {
            return *Component<C>::_p;
        }
    };

    template <typename C>
    struct PlainComponent : public BaseComponent {
        inline static u64 _id;
        inline static T *_p;
    };
    
    struct ComponentArray {
        static constexpr std::size_t BLOCK_SIZE = 512;

        struct Block {
            ComponentArray* parent = nullptr;

            std::size_t align = alignof(std::max_align_t);
            std::unique_ptr<std::byte[], void(*)(void*)> data{nullptr, nullptr};

            Block() = default;

            Block(ComponentArray* parent) : 
                parent(parent),
                align(parent->type->align),
                data(nullptr, [align = this->align](void* p) {
                    ::operator delete[](p, std::align_val_t(align));
                })
            {
                const std::size_t data_size = BLOCK_SIZE * this->parent->type->size;

                void* raw = ::operator new[](data_size, std::align_val_t(this->parent->type->align));
                data.reset(static_cast<std::byte*>(raw));

                std::memset(data.get(), 0, data_size);
            }

            BaseComponent* operator[](std::size_t i) const {
                assert(i < BLOCK_SIZE);
                return reinterpret_cast<BaseComponent*>(
                    data.get() + (this->parent->type->size * i)
                );
            }
        };

        ComponentType* type = nullptr;
        std::vector<Block> blocks;

        ComponentArray() = default;
        ComponentArray(ComponentType* type) : type(type) {}

        void resize(std::size_t n) {
            const std::size_t needed_blocks = (n + BLOCK_SIZE - 1) / BLOCK_SIZE;
            while (blocks.size() < needed_blocks) {
                blocks.emplace_back(this);
            }
        }

        BaseComponent* operator[](std::size_t i) const {
            return blocks[i / BLOCK_SIZE][i % BLOCK_SIZE];
        }

        inline std::size_t get_index(u8 block, const void* ptr) const {
            const std::byte* base = blocks[block].data.get();

            auto offset = reinterpret_cast<const std::byte*>(ptr) - base;
            std::size_t local_index = offset / this->type->size;

            return block * BLOCK_SIZE + local_index;
        }
    };

    template<typename... Components>
    requires (std::derived_from<Components, BaseComponent> && ...)
    struct View {
        View(T& p) : _p(p) {
            (_required_mask.set(Component<Components>::_id), ...);

            _arrays = std::make_tuple(
                &_p.template get_array<Components>()...
            );
        }

        struct Iterator {
            using set_iter = typename SerialSparseSet<Object>::iterator;

            struct EndTag {};

            Iterator(T& p,
                    set_iter iter,
                    const BitsetComp& mask,
                    std::array<ComponentArray*, sizeof...(Components)> arrays)
                : _p(p)
                , obj_iter(std::move(iter))
                , _required_mask(mask)
                , _arrays(arrays)
            {
                skip_invalid();
            }

            Iterator(T& p, set_iter iter, EndTag)
                : _p(p)
                , obj_iter(std::move(iter))
            {}

            auto operator*() const {
                const I id = (*obj_iter).get_id();
                return get_components(id, std::index_sequence_for<Components...>{});
            }

            Iterator& operator++() {
                ++obj_iter;
                skip_invalid();
                return *this;
            }

            bool operator!=(const Iterator& other) const {
                return obj_iter != other.obj_iter;
            }

            bool operator==(const Iterator& other) const {
                return obj_iter == other.obj_iter;
            }

        private:
            set_iter obj_iter;
            T& _p;
            BitsetComp _required_mask;
            std::array<ComponentArray*, sizeof...(Components)> _arrays;

            void skip_invalid() {
                while (obj_iter != _p.objects.end()) {
                    const I id = (*obj_iter).get_id();
                    const BitsetComp& mask = _p.components_mask[id];

                    if ((mask & _required_mask) == _required_mask) break;
                    ++obj_iter;
                }
            }

            template<std::size_t... Is>
            auto get_components(I id, std::index_sequence<Is...>) const {
                return std::tuple<Components&...>(
                    *reinterpret_cast<Components*>(
                        (*std::get<Is>(_arrays))[id]
                    )...
                );
            }
        };

        Iterator begin() {
            return Iterator(_p, _p.objects.begin(), _required_mask, _arrays);
        }

        Iterator end() {
            return Iterator(_p, _p.objects.end(), typename Iterator::EndTag{});
        }

    private:
        T& _p;
        BitsetComp _required_mask;
        std::array<ComponentArray*, sizeof...(Components)> _arrays;
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

        return components_mask[object.id][id];
    }

    template <typename C>
    inline C& add_component(const Object& object, C&& component) {
        static_assert(std::is_base_of_v<BaseComponent, C>);

        const u64 id = Component<C>::_id;
        auto& array = components[id];

        array.resize(object.id + 1);

        void* raw = array[object.id];
        C* ptr = std::launder(reinterpret_cast<C*>(raw));
        new (ptr) C(std::forward<C>(component));

        components_mask[object.id].set(id);
        ptr->block = object.id / ComponentArray::BLOCK_SIZE;

        return *ptr;
    }

    template <typename C>
    inline void remove_component(Object& object) {
        const u64 id = Component<C>::_id;
        auto& type = components_types[id];
        auto& array = components[id];

        void* ptr = array[object.id];
        type.destroy(ptr);

        components_mask[object.id].reset(id);
    }

    template <typename C>
    inline C& get_component(const Object& object) const {
        const u64 id = Component<C>::_id; 
        assert(components_mask[object.id][id] && "Component not present");
        return *reinterpret_cast<C*>(components[id][object.id]);
    }
    
    inline const SerialSparseSet<Object>& get_objects() const {return objects;}
  
    inline const Object& object_at(u64 i) const {return objects[i];}
    
    template <typename... Args>
    inline Object& create_object(Args&&... args) {
        u64 i = objects.push(Object(static_cast<T*>(this), invalid, std::forward<Args>(args)...));
        components_mask.resize(objects.size());

        Object& obj = objects[i];
        if constexpr (std::is_base_of_v<IObject, ObjBase>) obj.init();
        
        obj.id = i;
        return obj;
    }

    inline void remove_object(const Object& object) {
        BitsetComp& bitset = components_mask[object.id];
        iterate_bitset(bitset, [this, object_id = object.id](size_t component_id) {
            BaseComponent* component = components[component_id][object_id];
            components_types[component_id].destroy(component);
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

    SerialSparseSet<Object> objects;
    std::vector<BitsetComp> components_mask;
    u64 size = 0; 
};