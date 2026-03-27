#include <iostream>
#include "EntityManager.hpp"

int main(int, char**)
{
    // Example //
    
    ECS manager;
    manager.register_type<BasicComponent>();

    Entity& entity = manager.create_object();
    entity.number = 34;
    entity.add(BasicComponent(26));

    entity.get<BasicComponent>().print();

    return 0;
}