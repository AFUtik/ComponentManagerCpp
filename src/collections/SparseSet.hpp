#pragma once

#include <algorithm>
#include <array>
#include <limits>
#include <cassert>
#include <memory>

using u64 = std::uint64_t;

template<typename T, size_t N, typename I = u64>
struct StaticSparseSet {
    static constexpr I INVALID = std::numeric_limits<I>::max();
    static_assert(N < static_cast<size_t>(INVALID), "N exceeds index type range");

    template<typename U>
    I push(U&& data) {
        assert(_size < N);

        const I id = _size;

        dense   [id] = std::forward<U>(data);

        dense_id[id] = id;
        sparse  [id] = id;

        _size++;
        return id;
    }

    void erase(I id) {
        assert(contains(id));

        const I di      = sparse[id];
        const I last    = _size - 1;
        const I last_id = dense_id[last];

        dense   [di] = std::move(dense[last]);

        dense_id[di] = last_id;
        sparse[last_id] = di;

        sparse[id] = INVALID;
        _size--;
    }

    bool contains(I id) const {
        if (static_cast<size_t>(id) >= N) return false;
        return sparse[id] < _size;
    }

    T& operator[](I id) {
        assert(contains(id));
        return dense[sparse[id]];
    }

    const T& operator[](I id) const {
        assert(contains(id));
        return dense[sparse[id]];
    }

    I    size()     const { return _size; }
    bool empty()    const { return _size == 0; }
    bool full()     const { return _size == N; }
    
    static constexpr size_t capacity() { return N; }

    auto begin() { return dense.begin(); }
    auto end()   { return dense.begin() + _size; }

    auto begin() const { return dense.cbegin(); }
    auto end() const  { return dense.cbegin() + _size; }

    auto cbegin() const { return dense.cbegin(); }
    auto cend()   const { return dense.cbegin() + _size; }

    using iterator = std::array<T, N>::iterator;
    using const_iterator = std::array<T, N>::const_iterator;

    StaticSparseSet() {
        sparse.fill(INVALID);
    }

private:
    std::array<T, N> dense;
    std::array<I, N> dense_id;
    std::array<I, N> sparse;
    I _size = 0;
};

template<typename T, typename I = u64>
struct SparseSet {
    static constexpr I INVALID = std::numeric_limits<I>::max();

    template <typename U>
    I push(U&& data) {
        if (_size >= _capacity) {
            assert(_capacity != INVALID);
            allocate();
        }

        const I idx = _size;

        dense   [idx] = std::forward<U>(data);
        dense_id[idx] = idx;
        sparse  [idx] = idx;

        _size++;
        return idx;
    }

    void erase(I id) {
        assert(contains(id));
        
        const I dense_index = sparse[id];
        const I last_index  = _size - 1;
        const I last_id     = dense_id[last_index];

        dense   [dense_index] = std::move(dense[last_index]);
        dense_id[dense_index] = last_id;

        sparse[last_id] = dense_index;
        
        sparse[id] = INVALID;

        _size--;
    }

    bool contains(I id) const {
        if (static_cast<size_t>(id) >= static_cast<size_t>(_capacity)) 
            return false;
        return sparse[id] < _size;
    }

    T& operator[](I id) {
        assert(contains(id));
        return dense[sparse[id]];
    }

    const T& operator[](I id) const {
        assert(contains(id));
        return dense[sparse[id]];
    }

    I size() const { return _size; }

    struct iterator {
        using iterator_category = std::forward_iterator_tag;
        using value_type        = T;
        using reference         = T&;

        iterator(T* dense, I index) : dense(dense), index(index) {}

        T& operator*() { return dense[index]; }
        const T& operator*() const { return dense[index]; }
        
        iterator& operator++() { ++index; return *this; }
        
        bool operator!=(const iterator& other) const { return index != other.index; }
        bool operator==(const iterator& other) const { return index == other.index; }
    private:
        T* dense;
        I index;
    };

    struct const_iterator {
        using iterator_category = std::forward_iterator_tag;
        using value_type        = T;
        using reference         = const T&;

        const_iterator(const T* dense, I index) : dense(dense), index(index) {}

        const T& operator*() const { return dense[index]; }
        
        const_iterator& operator++() { ++index; return *this; }
        
        bool operator!=(const const_iterator& other) const { return index != other.index; }
        bool operator==(const const_iterator& other) const { return index == other.index; }
    private:
        const T* dense;
        I index;
    };

    iterator begin() { return {dense.get(), 0}; }
    iterator end()   { return {dense.get(), _size}; }

    const_iterator begin() const { return {dense.get(), 0}; }
    const_iterator end() const  { return {dense.get(), _size}; }

    const_iterator cbegin() const { return {dense.get(), 0}; }
    const_iterator cend()   const { return {dense.get(), _size}; }

    explicit SparseSet(I reserve = 0) : _capacity(reserve) {
        if (reserve > 0) allocate();
    }

private:
    void allocate() {
        const size_t new_cap = _capacity 
            ? static_cast<size_t>(_capacity) * 2 
            : 8;
        
        assert(new_cap <= static_cast<size_t>(INVALID));

        auto new_dense    = std::make_unique<T[]>(new_cap);
        auto new_dense_id = std::make_unique<I[]>(new_cap);
        auto new_sparse   = std::make_unique<I[]>(new_cap);

        if constexpr (std::is_trivially_copyable_v<T>) {
            std::memcpy(new_dense.get(), dense.get(), _size * sizeof(T));
        } else {
            std::move(dense.get(), dense.get() + _size, new_dense.get());
        }

        std::memcpy(new_dense_id.get(), dense_id.get(), _size * sizeof(I));
        std::memcpy(new_sparse.get(),   sparse.get(),   _size * sizeof(I));

        dense    = std::move(new_dense);
        dense_id = std::move(new_dense_id);
        sparse   = std::move(new_sparse);

        _capacity = static_cast<I>(new_cap);
    }

    std::unique_ptr<T[]> dense;
    std::unique_ptr<I[]> dense_id;
    std::unique_ptr<I[]> sparse;
    I _size = 0, _capacity = 0;
};