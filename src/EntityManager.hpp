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
    256, 
    256>
{
    ECS() : ComponentManager() {}
};

template <typename C>
using EntityComponent = ECS::Component<C>;
using Entity = ECS::Object;

struct BasicComponent : public ECS::Component<BasicComponent> {
    u64 number = 0;

    BasicComponent(u64 num) : number(num) {}

    void print() {
        const Entity& ent = this->object();
        std::cout << ent.number << std::endl;
    }
};