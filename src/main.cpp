#include <iostream>

#include "EntityManager.hpp"
#include "EventManager.hpp"

int main(int, char**)
{
    // Example //
    
    ECS manager;
    manager.register_type<BasicComponent>();

    Entity& entity = manager.create_object();
    Entity& entity2 = manager.create_object();

    EventId id;
    EventManager::subscribe<Entity, BasicEvent, &Entity::on_event>(&entity2, id);

    BasicEvent event;
    event.data = 25;
    
    EventManager::emit(event);

    std::cout << entity2.id << std::endl;

    return 0;
}