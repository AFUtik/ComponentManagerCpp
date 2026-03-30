#include <iostream>

#include "components/BasicComponent.hpp"

using ListenerId = u64;

int main(int, char**)
{
    // Example //
    ECS& ecs = ECS::instance();
    ecs.register_type<BasicComponent>();

    Entity& ent = ecs.create_object(); 
    ent.add(BasicComponent(64));
    
    auto view = ECS::View<BasicComponent>(ecs);
    for(auto [bs1] : view) {
        bs1.print();
    }

    return 0;
}