#pragma once

#include <array>

#include "Freelist.hpp"
#include "ComponentManager.hpp"

using u8  = std::uint8_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using usize = u64;

using EventId = u64;

class EventManager {
    template <typename Event>
    class EventBus {
    public:
        struct Listener {
            void* obj = nullptr;
            void(*call)(void*, const Event&) = nullptr;
        };

        Freelist<Listener, u64> listeners;

        u64 subscribe(void* obj, void(*call)(void*, const Event&)) {
            return listeners.push(Listener{obj, call});
        }

        inline void unsubscribe(u64 index) {
            listeners.erase(index);
        }

        void emit(const Event& e) {
            for (size_t i = 0; i < listeners.size(); ++i) {
                auto& listener = listeners[i];
                if(listener.call!=nullptr) listener.call(listener.obj, e);
            }
        } 
    private:
        EventManager* manager;
    };
public:
    template<typename Event>
    static auto& bus() {
        static EventBus<Event> instance;
        return instance;
    }

    template<typename Event>
    static void emit(const Event& e) {
        bus<Event>().emit(e);
    }

    template<typename T, typename Event, auto Method, typename I>
    static inline void subscribe(T* obj, I& id) {
        id = static_cast<I>(bus<Event>().subscribe(
            obj,
            [](void* o, const Event& e) {
                (static_cast<T*>(o)->*Method)(e);
            }
        ));
    }

}; 