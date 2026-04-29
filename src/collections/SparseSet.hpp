#pragma once

#include <algorithm>
#include <limits>
#include <cassert>
#include <memory>

using u64 = std::uint64_t;

template<typename T, typename I = u64>
struct SerialSparseSet {
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
    const_iterator end()   const { return {dense.get(), _size}; }

    explicit SerialSparseSet(I reserve = 0) : _capacity(reserve) {
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