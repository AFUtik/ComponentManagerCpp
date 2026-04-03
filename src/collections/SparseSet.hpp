#pragma once

#include <algorithm>
#include <limits>
#include <map>
#include <array>
#include <optional>
#include <vector>
#include <cassert>
#include <memory>
#include <iostream>

using u64 = std::uint64_t;

template<
    typename T,
    typename I,
    u64 Capacity
>
struct AllocatedSerialSparseSet {
    template<typename U>
    u64 push(U&& data) {
        assert(_size < Capacity);

        sparse[_size] = static_cast<I>(_size);
        dense[_size]  = std::forward<U>(data);
        _size++;

        return _size-1;
    }

    void erase(u64 id) {
        assert(id < _size);

        size_t index = sparse[id];
        size_t last_index = _size - 1;

        dense[index] = dense[last_index];

        sparse[last_index] = index;

        _size--;
    }

    bool contains(u64 id) const {
        size_t index = sparse[id];
        return index < _size;
    }

    u64 size() const {
        return _size;   
    }

    T& operator[](u64 id) {
        return dense[sparse[id]];
    }

    const T& operator[](u64 id) const {
        return dense[sparse[id]];
    }

    auto begin() {
        return dense.begin();
    }

    auto end() {
        return dense.begin() + _size;
    }
    
    auto begin() const {
        return dense.cbegin();
    }

    auto end() const {
        return dense.cbegin() + _size;
    }
private:
    std::array<T, Capacity> dense;
    std::array<I, Capacity> sparse;
    u64 _size = 0;
};

template<
    typename T,
    typename I,
    u64 Capacity
>
struct AllocatedSparseSet {
    template <typename U>
    void push(u64 id, U&& data) {
        assert(_size < Capacity);

        sparse[_size] = _size;
        dense[_size]  = std::forward<U>(data);
        dense_ids[_size] = static_cast<I>(id);
        _size++;
    }

    void erase(u64 id) {
        size_t index = sparse[id];
        size_t last = _size - 1;

        I last_id = dense_ids[last];

        dense[index] = dense[last];
        dense_ids[index] = last_id;

        sparse[last_id] = index;

        _size--;
    }

    bool contains(u64 id) const {
        size_t index = sparse[id];
        return index < _size && dense_ids[index] == id;
    }

    u64 size() const {
        return _size;   
    }

    T& operator[](u64 id) {
        return dense[sparse[id]];
    }

    const T& operator[](u64 id) const {
        return dense[sparse[id]];
    }

    auto begin() {
        return dense.begin();
    }

    auto end() {
        return dense.begin() + _size;
    }
    
    auto begin() const {
        return dense.cbegin();
    }

    auto end() const {
        return dense.cbegin() + _size;
    }
private:
    std::array<T, Capacity> dense;
    std::array<I, Capacity> dense_ids;
    std::array<I, Capacity> sparse;
    u64 _size = 0;
};

template<
    typename T,
    typename I = u64
>
struct SerialSparseSet {
    static constexpr u64 MAX = std::numeric_limits<I>::max();

    template <typename U>
    u64 push(U&& data) {
        if(_size >= _capacity) {
            assert(_capacity != MAX);
            allocate();
        }

        sparse[static_cast<u64>(_size)] = _size;
        dense[static_cast<u64>(_size)]  = std::forward<U>(data);
        _size++;

        return _size - 1;
    }

    void erase(u64 id) {
        size_t index = sparse[id];
        size_t last_index = _size - 1;

        dense[index] = dense[last_index];
        sparse[last_index] = index;

        _size--;
    }

    bool contains(u64 id) const {
        size_t index = sparse[id];
        return index < _size;
    }

    u64 size() const {
        return _size;   
    }

    T& operator[](u64 id) {
        return dense[sparse[id]];
    }

    const T& operator[](u64 id) const {
        return dense[sparse[id]];
    }

    struct Iterator {
        Iterator(std::unique_ptr<T[]>& dense, u64 index) : dense(dense), index(index) {}

        inline T& operator*() {
            return dense[index];
        }

        inline void operator++() {++index;}

        inline bool operator!=(const Iterator& other) const {return index != other.index;}
    private:
        std::unique_ptr<T[]>& dense;
        u64 index = 0;
    };

    struct ConstIterator {
        ConstIterator(const std::unique_ptr<T[]>& dense, u64 index) : dense(dense), index(index) {}

        inline const T& operator*() const {
            return dense[index];
        }

        inline void operator++() const {++index;}

        inline bool operator!=(ConstIterator& other) const {return index != other.index;}
    private:
        std::unique_ptr<T[]>& dense;
        mutable u64 index = 0;
    };

    auto begin() {return Iterator(dense, 0);}

    auto end() {return Iterator(dense, _size);}

    auto begin() const {return ConstIterator(dense, 0);}

    auto end() const {return ConstIterator(dense, _size);}

    SerialSparseSet(u64 reserve = 0) : _capacity(static_cast<I>(reserve)) {}
private:
    inline void allocate() {
        size_t new_capacity = _capacity ? _capacity * 2 : 1;

        auto new_dense  = std::make_unique<T[]>(new_capacity);
        auto new_sparse = std::make_unique<I[]>(new_capacity);

        if constexpr (std::is_trivially_copyable_v<T>) {
            std::memcpy(new_dense.get(), dense.get(), _size * sizeof(T));
        } else {
            std::move(dense.get(), dense.get() + _size, new_dense.get());
        }
        std::memcpy(new_sparse.get(), sparse.get(), _size * sizeof(I));

        dense  = std::move(new_dense);
        sparse = std::move(new_sparse);

        _capacity = new_capacity;
    }

    std::unique_ptr<T[]> dense;
    std::unique_ptr<I[]> sparse;
    I _size = 0, _capacity = 0;
};

template<
    typename T,
    typename I = u64
>
struct SparseSet {
    static constexpr u64 MAX = std::numeric_limits<I>::max();

    template <typename U>
    void push(U&& data, u64 index) {
        while(index >= _capacity_sparse) allocate_sparse();
        if(_size_dense >= _capacity_dense) allocate_dense();

        sparse[index] = _size_dense;
        dense[static_cast<u64>(_size_dense)] = std::forward<U>(data);
        dense_ids[static_cast<u64>(_size_dense)] = index;
        _size_dense++;
    }

    inline void erase(u64 id) {
        const size_t index = sparse[id];
        const size_t last_index = dense_ids[_size_dense - 1];

        dense[index] = std::move(dense[_size_dense - 1]);
        sparse[last_index] = index;

        _size_dense--;
    }

    bool contains(u64 id) const {
        size_t index = sparse[id];
        return index < _size_sparse;
    }

    u64 size() const {
        return _size_dense;   
    }

    T& operator[](u64 id) {
        return dense[sparse[id]];
    }

    const T& operator[](u64 id) const {
        return dense[sparse[id]];
    }

    struct Iterator {
        Iterator(std::unique_ptr<T[]>& dense, u64 index) : dense(dense), index(index) {}

        inline T& operator*() {
            return dense[index];
        }

        inline void operator++() {++index;}

        inline bool operator!=(const Iterator& other) const {return index != other.index;}
    private:
        std::unique_ptr<T[]>& dense;
        u64 index = 0;
    };

    struct ConstIterator {
        ConstIterator(const std::unique_ptr<T[]>& dense, u64 index) : dense(dense), index(index) {}

        inline const T& operator*() const {
            return dense[index];
        }

        inline void operator++() const {++index;}

        inline bool operator!=(ConstIterator& other) const {return index != other.index;}
    private:
        std::unique_ptr<T[]>& dense;
        mutable u64 index = 0;
    };

    auto begin() {return Iterator(dense, 0);}

    auto end() {return Iterator(dense, _size_dense);}

    auto begin() const {return ConstIterator(dense, 0);}

    auto end() const {return ConstIterator(dense, _size_dense);}

    SparseSet(u64 reserve = 0) : _capacity_dense(static_cast<I>(reserve)), _capacity_sparse(static_cast<I>(reserve)) {}
private:
    inline void allocate_sparse() {
        size_t new_capacity = _capacity_sparse ? _capacity_sparse * 2 : 1;
        auto new_sparse  = std::make_unique<T[]>(new_capacity);
        std::memcpy(new_sparse.get(), sparse.get(), _size_sparse * sizeof(I));
        sparse = std::move(new_sparse);
        _capacity_sparse = new_capacity;
    }

    inline void allocate_dense() {
        size_t new_capacity = _capacity_dense ? _capacity_dense * 2 : 1;
        auto new_dense     = std::make_unique<T[]>(new_capacity);
        auto new_dense_ids = std::make_unique<T[]>(new_capacity);

        if constexpr (std::is_trivially_copyable_v<T>) {
            std::memcpy(new_dense.get(), dense.get(), _size_dense * sizeof(T));
        } else {
            std::move(dense.get(), dense.get() + _size_dense, new_dense.get());
        }
        std::memcpy(new_dense_ids.get(), dense_ids.get(), _size_dense * sizeof(I));

        dense     = std::move(new_dense);
        dense_ids = std::move(new_dense_ids);
        _capacity_dense = new_capacity;
    }

    std::unique_ptr<T[]> dense;
    std::unique_ptr<I[]> dense_ids; 
    std::unique_ptr<I[]> sparse;
    I _size_dense  = 0;
    I _size_sparse = 0;
    I _capacity_dense  = 0;
    I _capacity_sparse = 0;
};


template<
    typename T,
    typename I = u64
>
struct FlatDenseMap {
    static constexpr u64 MAX = std::numeric_limits<I>::max();

    template <typename U>
    inline void push(I key, U&& data) {
        if(_size >= _capacity) {
            assert(_capacity != MAX);
            allocate();
        }

        dense[static_cast<u64>(_size)]  = std::forward<U>(data);
        dense_ids[static_cast<u64>(_size)] = key;
        insert_flatmap(key, _size);

        _size++;
    }

    void erase(I key) {
        auto it = find_flatmap(key);
        assert(it);

        size_t index = *it;
        size_t last_index = _size - 1;

        I last_id = dense_ids[last_index];

        dense[index] = std::move(dense[last_index]);
        dense_ids[index] = last_id;
        
        insert_flatmap(last_id, index);
        erase_flatmap(key);

        _size--;
    }

    bool contains(I key) const {
        return find_flatmap(key) != MAX;
    }

    u64 size() const {
        return _size;   
    }

    T& operator[](I key) {
        auto idx = find_flatmap(key);
        assert(idx != MAX);
        return dense[idx];
    }

    const T& operator[](I key) const {
        auto idx = find_flatmap(key);
        assert(idx != MAX);
        return dense[idx];
    }

    struct Iterator {
        Iterator(T* dense, u64 index) : dense(dense), index(index) {}

        inline T& operator*() {
            return dense[index];
        }

        inline void operator++() {++index;}

        inline bool operator!=(const Iterator& other) const {return index != other.index;}
    private:
        T* dense  = nullptr;
        u64 index = 0;
    };

    struct ConstIterator {
        ConstIterator(const T* dense, u64 index) : dense(dense), index(index) {}

        inline const T& operator*() const {
            return dense[index];
        }

        inline void operator++() const {++index;}

        inline bool operator!=(ConstIterator& other) const {return index != other.index;}
    private:
        const T* dense = nullptr;
        mutable u64 index = 0;
    };

    auto begin() {return Iterator(dense.get(), 0);}

    auto end() {return Iterator(dense.get(), _size);}

    auto begin() const {return ConstIterator(dense.get(), 0);}

    auto end() const {return ConstIterator(dense.get(), _size);}

    FlatDenseMap(u64 capacity_reserve = 0) {
        if (capacity_reserve) {
            _capacity = capacity_reserve;
            dense = std::make_unique<T[]>(_capacity);
            dense_ids = std::make_unique<I[]>(_capacity);
            sparse_flatmap = std::make_unique<std::pair<I,I>[]>(_capacity);
        }
    }
private:
    inline void allocate() {
        size_t new_capacity = _capacity ? _capacity * 2 : 1;

        auto new_dense     = std::make_unique<T[]>(new_capacity);
        auto new_dense_ids = std::make_unique<I[]>(new_capacity);
        auto new_sparse_flatmap = std::make_unique<std::pair<I, I>[]>(new_capacity);

        if constexpr (std::is_trivially_copyable_v<T>) {
            std::memcpy(new_dense.get(), dense.get(), _size * sizeof(T));
        } else {
            std::move(dense.get(), dense.get() + _size, new_dense.get());
        }

        std::memcpy(new_dense_ids.get(),  dense_ids.get(), _size * sizeof(I));
        std::memcpy(new_sparse_flatmap.get(), sparse_flatmap.get(), _size * sizeof(std::pair<I, I>));
        
        dense     = std::move(new_dense);
        dense_ids = std::move(new_dense_ids);
        sparse_flatmap = std::move(new_sparse_flatmap);
        
        _capacity = new_capacity;
    }

    // flatmap funcs

    /* O(N) */
    inline void insert_flatmap(I key, I value) {
        auto begin = sparse_flatmap.get();
        auto end   = begin + _size;

        auto it = std::lower_bound(
            begin, end, key,
            [](const auto& p, I k) {
                return p.first < k;
            }
        );

        if (it != end && it->first == key) {
            it->second = value;
            return;
        }

        // shift //
        size_t index = it - begin;
        for (size_t i = _size; i > index; --i) {
            sparse_flatmap[i] = std::move(sparse_flatmap[i - 1]);
        }
        sparse_flatmap[index] = {key, value};
    }

    /* O(N) */
    inline bool erase_flatmap(I key) {
        auto begin = sparse_flatmap.get();
        auto end   = begin + _size;

        auto it = std::lower_bound(
            begin, end, key,
            [](const auto& p, I k) {
                return p.first < k;
            }
        );

        if (it == end || it->first != key) return false;

        size_t index = it - begin;
        for (size_t i = index; i < _size - 1; ++i) {
            sparse_flatmap[i] = std::move(sparse_flatmap[i + 1]);
        }
        return true;
    }

    /* O(log N) */
    inline I find_flatmap(I key) {
        auto begin = sparse_flatmap.get();
        auto end   = begin + _size;

        auto it = std::lower_bound(
            begin, end, key,
            [](const auto& p, I k) {
                return p.first < k;
            }
        );

        if (it != end && it->first == key) return it->second;
        
        return MAX;
    }

    std::unique_ptr<T[]> dense;
    std::unique_ptr<I[]> dense_ids;
    std::unique_ptr<std::pair<I, I>[]> sparse_flatmap;
    I _size = 0, _capacity = 0;
};