#pragma once

#include <array>
#include <cassert>
#include <cstdint>
#include <limits>
#include <memory>
#include <type_traits>
#include <utility>
#include <array>
#include <new>
#include <concepts>
#include <cstdint>
#include <vector>

#include "collections/SparseSet.hpp"

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using usize = u64;

struct Empty {};

template <
    typename T,
    typename I,
    typename ObjBase = Empty,
    size_t MAX_COMPONENTS = 64,
    size_t COMPONENTS_ALLOC = 256>
struct ComponentManager {
    static constexpr I invalid = std::numeric_limits<I>::max();
    
    struct Object : public ObjBase {
        Object() = default;
        Object(I id) : id(id) {}
        
        template <typename... Args>
        Object(I id, Args&&... args) : ObjBase(std::forward<Args>(args)...), id(id) {}

        bool valid() { return id < invalid;}

        I get_id() {return id;}
    private:
        I id = invalid;
    };

    struct ObjectView
    {
        Object object;
        
        ObjectView(const Object& object, T* manager) : object(object), _p(manager) {}

        template <typename C>
        C& get() const {
            return _p->template get_component<C>(*this);
        }

        template <typename C>
        void add(C&& component) {
            _p->template add_component<C>(*this, std::forward<C>(component));   
        }

        template <typename C>
        void remove() {
            _p->template remove_component<C>(*this);   
        }

        void destroy() {
            _p->remove_object(*this);
        }
    private:
        T* _p = nullptr;
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

    struct ComponentMaskStorage {
        ComponentMaskStorage() = default;

        void resize(std::size_t count) {
            objects_count = count;
            data.resize(count * words_per_object);
        }

        bool has_mask(std::size_t obj_id, std::size_t comp_id) const {
            std::size_t word_idx = comp_id / 64;
            std::size_t bit_idx = comp_id % 64;
            return (word_at(obj_id, word_idx) >> bit_idx) & 1;
        }

        void set_mask(std::size_t obj_id, std::size_t comp_id) {
            std::size_t word_idx = comp_id / 64;
            std::size_t bit_idx = comp_id % 64;
            word_at(obj_id, word_idx) |= (1ULL << bit_idx);
        }

        void reset_mask(std::size_t obj_id, std::size_t comp_id) {
            std::size_t word_idx = comp_id / 64;
            std::size_t bit_idx = comp_id % 64;
            
            word_at(obj_id, word_idx) &= ~(1ULL << bit_idx);
        }

        void clear_masks(std::size_t obj_id) {
            for (std::size_t w = 0; w < words_per_object; ++w) {
                word_at(obj_id, w) = 0;
            }
        }
    private:
        uint64_t& word_at(std::size_t obj_id, std::size_t word_idx) {
            return data[obj_id * words_per_object + word_idx];
        }        

        std::vector<uint64_t> data;
        std::size_t objects_count = 0;
        const std::size_t words_per_object = (MAX_COMPONENTS + 63) / 64;
    };

    template<typename... Components>
    requires (std::derived_from<Components, BaseComponent> && ...)
    struct View {
        View(T& p) : _p(p), _words_per_object((MAX_COMPONENTS + 63) / 64) {
           _required_mask.resize(_words_per_object, 0);
        
            (_set_required_bit(Component<Components>::_id), ...);

            _arrays = std::make_tuple(
                &_p.template get_array<Components>()...
            );
        }

        struct iterator {
            using const_set_iter = typename SerialSparseSet<Object>::ConstIteraor ;

            struct EndTag {};

            iterator(T& p,
                    const_set_iter iter,
                    const std::vector<u64>& mask,
                    const std::array<ComponentArray*, sizeof...(Components)> &arrays)
                : _p(p)
                , obj_iter(std::move(iter))
                , _required_mask(mask)
                , _arrays(arrays)
            {
                skip_invalid();
            }

            iterator(T& p, const_set_iter iter, EndTag)
                : _p(p)
                , obj_iter(std::move(iter))
            {}

            auto operator*() const {
                const I id = (*obj_iter).get_id();
                return get_components(id, std::index_sequence_for<Components...>{});
            }

            iterator& operator++() {
                ++obj_iter;
                skip_invalid();
                return *this;
            }

            bool operator!=(const iterator& other) const {
                return obj_iter != other.obj_iter;
            }

            bool operator==(const iterator& other) const {
                return obj_iter == other.obj_iter;
            }

        private:
            const_set_iter obj_iter;
            T& _p;
            const std::vector<u64>& _required_mask;
            std::array<ComponentArray*, sizeof...(Components)> _arrays;

            void skip_invalid() {
            while (obj_iter != _p.objects.end()) {
                const I id = (*obj_iter).get_id();
                
                bool has_all = true;
                for (std::size_t w = 0; w < _required_mask.size(); ++w) {
                    u64 obj_word = _p.components_mask.get_word(id, w);
                    if ((obj_word & _required_mask[w]) != _required_mask[w]) {
                        has_all = false;
                        break;
                    }
                }
                
                if (has_all) break;
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

        iterator begin() {
            return Iterator(_p, _p.objects.begin(), _required_mask, _arrays);
        }

        iterator end() {
            return Iterator(_p, _p.objects.end(), typename iterator::EndTag{});
        }

    private:
        T& _p;
        std::vector<u64> _required_mask;
        std::size_t _words_per_object;
        std::array<ComponentArray*, sizeof...(Components)> _arrays;

        void _set_required_bit(u64 comp_id) {
            std::size_t word_idx = comp_id / 64;
            std::size_t bit_idx = comp_id % 64;
            _required_mask[word_idx] |= (1ULL << bit_idx);
        }
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
        this->components[id].resize(COMPONENTS_ALLOC);
    }
    
    template <typename C>
    inline bool has_component(const Object& object) {
        static_assert(std::is_base_of_v<BaseComponent, C>);

        const u64 id = Component<C>::_id;
        auto& array = components[id];

        return maskStorage.has_mask(object.id, id); //components_mask[object.id][id];
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

        //components_mask[object.id].set(id);
        maskStorage.set_mask(object.id, id);
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

        //components_mask[object.id].reset(id);
        maskStorage.reset_mask(object.id, id);
    }

    template <typename C>
    inline C& get_component(const Object& object) const {
        const u64 id = Component<C>::_id; 
        //assert(components_mask[object.id][id] && "Component not present");
        return *reinterpret_cast<C*>(components[id][object.id]);
    }
    
    inline const SerialSparseSet<Object>& get_objects() const {return objects;}
  
    inline const Object& object_at(u64 i) const {return objects[i];}

    template <typename... Args>
    inline Object& create_object(Args&&... args) {
        u64 i = objects.push(Object(invalid, std::forward<Args>(args)...));

        Object& obj = objects[i];

        obj.id = i;
        return obj;
    }

    inline void remove_object(const Object& object) {
        objects.erase(object.id);
    }
private:
    template <typename C> ComponentArray& get_array() {
        const u64 id = Component<C>::_id;
        return components[id];
    }

    std::array<ComponentArray, MAX_COMPONENTS> components;
    std::array<ComponentType,  MAX_COMPONENTS> components_types;
    u64 components_cnt = 0;

    SerialSparseSet<Object> objects;
    ComponentMaskStorage maskStorage;
};