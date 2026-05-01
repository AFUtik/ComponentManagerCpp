#pragma once

#include <cassert>
#include <cstring>
#include <cstddef>
#include <type_traits>
#include <utility>
#include <iostream>

#include "collections/SparseSet.hpp"

template<typename Sig>
class Delegate;
 
template<typename Ret, typename... Args>
class Delegate<Ret(Args...)>
{
    static constexpr std::size_t SBO = 2 * sizeof(void*);
 
    alignas(void*) char   _buf[SBO]{};
    Ret (*_invoke)(const char*, Args&&...) = nullptr;
    void (*_copy)(char*, const char*)      = nullptr;
 
    template<Ret(*Fn)(Args...)>
    struct FreeFn {
        static Ret invoke(const char*, Args&&... args) {
            return Fn(std::forward<Args>(args)...);
        }
        static void copy(char*, const char*) {}
    };
 
    template<typename T, Ret(T::*Mem)(Args...)>
    struct MemFn {
        struct State { T* obj; };
        static_assert(sizeof(State) <= SBO,
            "Pointer size exceeds SBO buffer (impossible on standard platforms)");
 
        static Ret invoke(const char* buf, Args&&... args) {
            const auto& s = *reinterpret_cast<const State*>(buf);
            return (s.obj->*Mem)(std::forward<Args>(args)...);
        }
        static void copy(char* dst, const char* src) {
            std::memcpy(dst, src, sizeof(State));
        }
    };
 
    template<typename T, Ret(T::*Mem)(Args...) const>
    struct ConstMemFn {
        struct State { const T* obj; };
        static_assert(sizeof(State) <= SBO, "");
 
        static Ret invoke(const char* buf, Args&&... args) {
            const auto& s = *reinterpret_cast<const State*>(buf);
            return (s.obj->*Mem)(std::forward<Args>(args)...);
        }
        static void copy(char* dst, const char* src) {
            std::memcpy(dst, src, sizeof(State));
        }
    };
 
    template<typename Fn>
    struct StatelessFn {
        static Ret invoke(const char*, Args&&... args) {
            return Fn{}(std::forward<Args>(args)...);
        }
        static void copy(char*, const char*) {}
    };
 
public:
    Delegate() = default;
 
    Delegate(const Delegate& o) noexcept {
        if (o._copy) o._copy(_buf, o._buf);
        _invoke = o._invoke;
        _copy   = o._copy;
    }

    Delegate& operator=(const Delegate& o) noexcept {
        if (this != &o) {
            if (o._copy) o._copy(_buf, o._buf);
            _invoke = o._invoke;
            _copy   = o._copy;
        }
        return *this;
    }
 
    template<typename T, auto Mem, typename = void>
    struct IsConstMem : std::false_type {};
 
    template<typename T, auto Mem>
    struct IsConstMem<T, Mem,
        std::void_t<decltype(static_cast<Ret(T::*)(Args...) const>(Mem))>>
        : std::true_type {};

    template<Ret(*Fn)(Args...)>
    static Delegate from() noexcept {
        Delegate d;
        d._invoke = &FreeFn<Fn>::invoke;
        d._copy   = &FreeFn<Fn>::copy;
        return d;
    }
 
    template<auto Mem, typename T>
    static Delegate from(T* obj) noexcept {
        if constexpr (IsConstMem<T, Mem>::value) {
            using MF = ConstMemFn<T, Mem>;
            typename MF::State state{obj};
            Delegate d;
            std::memcpy(d._buf, &state, sizeof(state));
            d._invoke = &MF::invoke;
            d._copy   = &MF::copy;
            return d;
        } else {
            using MF = MemFn<T, Mem>;
            typename MF::State state{obj};
            Delegate d;
            std::memcpy(d._buf, &state, sizeof(state));
            d._invoke = &MF::invoke;
            d._copy   = &MF::copy;
            return d;
        }
    }

    template<typename Fn>
    static Delegate from_fn(Fn) noexcept {
        static_assert(std::is_empty_v<Fn>,
            "Only stateless (captureless) lambdas/functors are supported. "
            "For capturing lambdas use Event with std::function or store state in a struct.");
        Delegate d;
        d._invoke = &StatelessFn<Fn>::invoke;
        d._copy   = &StatelessFn<Fn>::copy;
        return d;
    }
 
    Ret operator()(Args... args) const {
        assert(_invoke && "Calling empty Delegate");
        return _invoke(_buf, std::forward<Args>(args)...);
    }
 
    explicit operator bool() const noexcept { return _invoke != nullptr; }
 
    void Reset() noexcept {
        _invoke = nullptr;
        _copy   = nullptr;
    }
};
 
template<typename Sig, std::size_t N = 0>
class Event;
 
template<typename Ret, typename... Args, std::size_t N>
class Event<Ret(Args...), N>
{
    using D = Delegate<Ret(Args...)>;
    using EventSet = std::conditional_t<N == 0, SparseSet<D>, StaticSparseSet<D, N>>;

    EventSet _listeners;
public:
    std::size_t operator+=(D d) noexcept {
        if constexpr (N > 0)
            assert(_listeners.size() < N && "Event: listener array is full, increase N");
        return _listeners.push(d);
    }

    void Remove(std::size_t handle) noexcept {
        assert(_listeners.contains(handle) && "Invalid listener handle");
        _listeners.erase(handle);
    }

    void Emit(Args... args) const {
        for (auto& listener : _listeners) listener(args...);
    }

    void operator()(Args... args) const { Emit(args...); }

    std::size_t size()  const noexcept { return _listeners.size(); }
    bool        empty() const noexcept { return _listeners.empty(); }
};

struct DamageEvent
{
    int damage;
};

struct Player {
    const char* name;

    void onDamage(const DamageEvent& event) {
        std::cout << event.damage << std::endl;
    }
};

void testEvents() {
    using DmgEvent  = Event<void(const DamageEvent&), 0>;
    using DmgDelegate = Delegate<void(const DamageEvent&)>;
    DmgEvent onDamage;
 
    std::cout << "Delegate Sizeof: " << sizeof(DmgDelegate) << std::endl;

    Player player{"Hero"};
 
    auto h1 = onDamage += DmgDelegate::from<&Player::onDamage>(&player);

    onDamage.Emit({ 25 });
}