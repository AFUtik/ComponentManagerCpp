#include <iostream>

#include "EventManager.hpp"
#include "collections/SparseSet.hpp"
#include "components/BasicComponent.hpp"

using SubId = u16;

int main(int, char**)
{
    // Example //
    ECS& ecs = ECS::instance();
    ecs.register_type<BasicComponent>();

    Entity& ent = ecs.create_object(25); 
    ent.add(BasicComponent());

    std::cout << ent.number << std::endl;

    EventManager::emit(BasicEvent(101));
    
    return 0;
}