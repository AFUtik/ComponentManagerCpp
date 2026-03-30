#include <iostream>

#include "components/BasicComponent.hpp"

using ListenerId = u64;

int main(int, char**)
{
    // Example //
    ECS& ecs = ECS::instance();
    ecs.register_type<BasicComponent>();

    Entity& ent = ecs.create_object(); 
    ent.number = 25;
    ent.add(BasicComponent(64));
    
    for(const auto& obj : ecs.get_objects()) {
        std::cout << obj.number << std::endl;
    }

    return 0;
}