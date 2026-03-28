#include <iostream>
#include "EntityManager.hpp"

int main(int, char**)
{
    // Example //
    
    ECS manager;
    manager.register_type<BasicComponent>();

    Entity& entity = manager.create_object();
    Entity& entity2 = manager.create_object();

    std::cout << entity2.id << std::endl;

    return 0;
}