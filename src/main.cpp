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

    Entity& ent = ecs.create_object(); 
    ent.add(BasicComponent());
    ent.number = 25;
    
    //ListenerId id;
    //EventManager::subscribe<Entity, BasicEvent, &Entity::on_event>(&ent, id);
    //EventManager::unsubscribe<BasicEvent>(id);
    //EventManager::emit(BasicEvent(34));

    EventManager::emit(BasicEvent(101));

    auto view = ECS::View<BasicComponent>(ecs);
    for(auto [bs] : view) {
        std::cout << "okay" << std::endl;
    }
    
    return 0;
}