#include <iostream>

#include "components/BasicComponent.hpp"
#include "Freelist.hpp"

using ListenerId = u64;

int main(int, char**)
{
    // Example //
    ECS& ecs = ECS::instance();
    ecs.register_type<BasicComponent>();

    Entity& ent = ecs.create_object(); 

    ent.add(BasicComponent(64));
    ecs.remove_object(ent);

    std::cout << "Size of ECS: " << (sizeof(ecs) / 1024) << " kb" << std::endl;

    return 0;
}