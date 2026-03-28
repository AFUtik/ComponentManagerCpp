#pragma once

#include <array>
#include <vector>
#include <limits>
#include <vcruntime_typeinfo.h>

using u64 = std::size_t;

template<
    typename T,
    typename I,
    u64 Capacity    
>
class AllocatedFreelist {
public:
    static constexpr I invalid = std::numeric_limits<I>::max();
    
    AllocatedFreelist() {
        for(int i = 0; i < Capacity; i++) next[i]=i+1;
    }

    inline I push(T&& object) {
        I index = obtain_free_index();
        array[index] = std::forward<T>(object);
        return index;
    }

    inline void erase(u64 index, T set_value = T()) {
        array[index] = set_value;
        next[index] = free_head;
        free_head = index;
    }

    inline T& operator[](u64 index) {
        return array[index];
    }

    inline const T& operator[](u64 index) const {
        return array[index];
    }

    inline void fill(T data) {
        std::memset(array.data(), data, Capacity * sizeof(T*));
    }
    
    inline u64 obtain_free_index() {
        assert(free_head!=invalid);

        u64 idx = free_head;
        free_head = next[idx]; 
        return idx; 
    }
private:
    std::array<T, Capacity> array;
    std::array<I, Capacity> next;
    I free_head = 0;
};

template<
    typename T,
    typename I 
> 
class Freelist 
{
public:
    static constexpr I invalid = std::numeric_limits<I>::max();

    Freelist() {
        vec.reserve(256);
        next.reserve(256);
    }

    inline I push(T&& object) {
        I index = obtain_free_index();
        if (index == _size) {
            vec.emplace_back(std::forward<T>(object));
            next.emplace_back(invalid);
            _size++;
        } else {
            vec[index] = std::forward<T>(object);
        }
        return index;
    }

    inline void erase(I index) {
        assert(index >= _size);
        if (index == _size - 1) {
            vec.pop_back();
            next.pop_back();
            _size--;

            while (_size > 0 && free_head == _size) {
                free_head = next[free_head];
                vec.pop_back();
                next.pop_back();
                _size--;
            }

            if (_size == 0) {
                free_head = invalid;
            }

            return;
        }
        vec[index] = T{};
        next[index] = free_head;
        free_head = index;
    }

    inline T& operator[](u64 index) {
        return vec[index];
    }

    inline const T& operator[](u64 index) const {
        return vec[index];
    }

    inline u64 size() {return _size;}

    inline u64 obtain_free_index() {
        if (free_head != invalid) { 
            uint32_t idx = free_head; 
            free_head = next[idx]; 
            return idx;
        } 
        return _size; 
    }
private:
    std::vector<T> vec;
    std::vector<I> next;
    u64 free_head = invalid;
    u64 _size = 0;
};