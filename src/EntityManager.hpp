#include "ComponentManager.hpp"
#include <iostream>

struct BasicEvent {
    int data;
};

struct EntityObj {
    u64 number;

    void on_event(const BasicEvent& event) {
        std::cout << event.data << std::endl;
    };
};

struct ECS : ComponentManager<
    ECS, 
    u32, 
    EntityObj, 
    64, 
    256>
{
    ECS() : ComponentManager() {}

    static ECS& instance() {
        static ECS manager;
        return manager;
    }
};

template <typename C>
using EntityComponent = ECS::Component<C>;
using Entity = ECS::Object;
