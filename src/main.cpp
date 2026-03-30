#include <iostream>

#include "EntityManager.hpp"
#include "EventManager.hpp"
#include "Freelist.hpp"

using ListenerId = u64;

int main(int, char**)
{
    // Example //

    ECS& ecs = ECS::instance();
    Entity& obj = ecs.create_object();

    ecs.register_type<BasicComponent>();
    ecs.register_type<BasicComponent2>();

    obj.add(BasicComponent(34));
    obj.add(BasicComponent2(35));

    auto view = ECS::View<BasicComponent, BasicComponent2>(&ecs);
    for(auto [b1, b2] : view) {
        b1.print();
        b2.print();
    }

    return 0;
}