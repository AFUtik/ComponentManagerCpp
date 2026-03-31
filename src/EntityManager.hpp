#include "ComponentManager.hpp"
#include <iostream>

struct BasicEvent {
    int data;
};

struct EntityObj : public IObject {
    u64 number;

    EntityObj() = default;
    EntityObj(u64 number) : number(number) {}
    
    void init() override {
        std::cout << "Entity Init" << std::endl;   
    }

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

    static ECS& instance() {
        static ECS manager;
        return manager;
    }
};

template <typename C>
using EntityComponent = ECS::Component<C>;
using Entity = ECS::Object;
