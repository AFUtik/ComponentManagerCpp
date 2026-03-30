#pragma once

#include <array>
#include <vector>

using u64 = std::uint64_t;

template<
    typename T,
    typename I,
    u64 Capacity
>
struct AllocatedSerialSparseSet {
    u64 push(T& data) {
        assert(_size < Capacity);

        sparse[_size] = static_cast<I>(_size);
        dense[_size]  = std::move(data);
        _size++;

        return _size-1;
    }

    u64 push(T&& data) {
        assert(_size < Capacity);

        sparse[_size] = static_cast<I>(_size);
        dense[_size]  = std::forward<T>(data);
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

    struct Iterator {
        Iterator(std::array<T, Capacity>& dense, u64 index) : dense(dense), index(index) {}

        inline T& operator*() {
            return dense[index];
        }

        inline void operator++() {++index;}

        inline bool operator!=(const Iterator& other) const {return index != other.index;}
    private:
        std::array<T, Capacity>& dense;
        u64 index = 0;
    };

    struct ConstIterator {
        ConstIterator(const std::array<T, Capacity>& dense, u64 index) : dense(dense), index(index) {}

        inline const T& operator*() const {return dense[index];}

        inline void operator++() const {++index;}

        inline bool operator!=(ConstIterator& other) const {return index != other.index;}
    private:
        const std::array<T, Capacity>& dense;
        mutable u64 index = 0;
    };

    Iterator begin() {
        return Iterator(dense, 0);
    }

    Iterator end() {
        return Iterator(dense, _size);
    }
    
    ConstIterator begin() const {
        return ConstIterator(dense, 0);
    }

    ConstIterator end() const {
        return ConstIterator(dense, _size);
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
    void push(u64 id, T& data) {
        assert(_size < Capacity);

        sparse[id] = static_cast<I>(_size);
        dense[_size]  = std::move(data);
        dense_ids[_size] = static_cast<I>(id);
        _size++;
    }

    void push(u64 id, T&& data) {
        assert(_size < Capacity);

        sparse[_size] = _size;
        dense[_size]  = std::forward<T>(data);
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

    struct Iterator {
        Iterator(std::array<T, Capacity>& dense, u64 index) : dense(dense), index(index) {}

        inline T& operator*() {
            return dense[index];
        }

        inline const T& operator*() const {
            return dense[index];
        }

        inline void operator++() {++index;}

        inline bool operator!=(const Iterator& other) const {return index != other.index;}
    private:
        std::array<T, Capacity>& dense;
        u64 index = 0;
    };

    Iterator begin() {return Iterator(dense, 0);}

    Iterator end() {return Iterator(dense, _size);}

    const Iterator begin() const {return Iterator(dense, 0);}

    const Iterator end() const {return Iterator(dense, _size);}
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
    u64 push(T& data) {
        sparse.push_back(static_cast<I>(dense.size()));
        dense.push_back(std::move(data));
        return dense.size() - 1;
    }

    u64 push(T&& data) {
        sparse.push_back(static_cast<I>(dense.size()));
        dense.push_back(std::forward<T>(data));
        return dense.size() - 1;
    }

    void erase(u64 id) {
        size_t index = sparse[id];
        size_t last_index = dense.size() - 1;

        dense[index] = dense[last_index];
        sparse[last_index] = index;

        dense.pop_back();
    }

    bool contains(u64 id) const {
        size_t index = sparse[id];
        return index < dense.size();
    }

    u64 size() const {
        return dense.size();   
    }

    T& operator[](u64 id) {
        assert(id < sparse.size());

        return dense[sparse[id]];
    }

    const T& operator[](u64 id) const {
        assert(id < sparse.size());

        return dense[sparse[id]];
    }

    auto begin() {return dense.begin();}

    auto end() {return dense.end();}
private:
    std::vector<T> dense;
    std::vector<I> sparse;
};