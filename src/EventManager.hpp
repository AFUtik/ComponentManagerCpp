#pragma once

#include <array>
#include <bitset>
#include <bit>
#include <limits>

#include "collections/SparseSet.hpp"
#include "collections/Freelist.hpp"

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using usize = u64;

template <typename T, typename... Ts>
struct index_of;

template <typename T, typename... Ts>
struct index_of<T, T, Ts...> { static constexpr size_t value = 0; };

template <typename T, typename U, typename... Ts>
struct index_of<T, U, Ts...> { static constexpr size_t value = 1 + index_of<T, Ts...>::value;};

//template<typename T, typename I, u64 MAX_EVENTS>
struct EventManager {
    //using BitsetEvents = std::bitset< (MAX_EVENTS + 63) & ~size_t(63) >;

    template <typename Event>
    class EventBus {
    public:
        struct Listener {
            void* obj = nullptr;
            void(*call)(void*, const Event&) = nullptr;
        };

        SerialSparseSet<Listener, u64> listeners;

        u64 subscribe(void* obj, void(*call)(void*, const Event&)) {
            return listeners.push(Listener{obj, call});
        }

        inline void unsubscribe(u64 index) {
            listeners.erase(index);
        }

        void emit(const Event& e) {
            for (auto& listener : listeners) listener.call(listener.obj, e);
        }
    };
    
    template <typename T, typename I = u32, typename... Events>
    struct Subscription {
        static constexpr I invalid = std::numeric_limits<I>::max();
        
        template <typename E>
        static constexpr size_t index() {
            return index_of<E, Events...>::value;
        }

        template <typename Event, auto Method>
        void add(T* obj) 
        {
            if(data[index<Event>()] != invalid) 
            {
                remove<Event>();
            }
            
            data[index<Event>()] = EventManager::subscribe<T, Event, Method, I>(obj);
        }

        template <typename Event>
        void remove()
        {
            const I id = data[index<Event>()];
            assert(id!=invalid);
            EventManager::unsubscribe<Event>(id);
            data[index<Event>()] = invalid;
        }

        
    private:
        std::array<I, sizeof...(Events)> data{invalid};
    };

    template<typename Event>
    static inline auto& bus() {
        static EventBus<Event> instance;
        return instance;
    }

    template<typename Event>
    static inline void emit(const Event& e) {
        bus<Event>().emit(e);
    }

    template<typename T, typename Event, auto Method, typename I>
    static inline I subscribe(T* obj) {
        return static_cast<I>(bus<Event>().subscribe(
            obj,
            [](void* o, const Event& e) {
                (static_cast<T*>(o)->*Method)(e);
            }
        ));
    }

    template<typename Event>
    static inline void unsubscribe(u64 listener_id) {
        bus<Event>().unsubscribe(listener_id);
    }
private:

}; 