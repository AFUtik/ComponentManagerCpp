#include <iostream>

#include "EventManager.hpp"
#include "collections/SparseSet.hpp"
#include "components/BasicComponent.hpp"

using ListenerId = u64;

int main(int, char**)
{
    // Example //
    ECS& ecs = ECS::instance();
    ecs.register_type<BasicComponent>();

    Entity& ent = ecs.create_object(); 
    ent.number = 25;
    
    //ListenerId id;
    //EventManager::subscribe<Entity, BasicEvent, &Entity::on_event>(&ent, id);
    //EventManager::unsubscribe<BasicEvent>(id);
    //EventManager::emit(BasicEvent(34));

    FlatDenseMap<u16, u16> sparse_set;
    sparse_set.push(4993, 1);
    sparse_set.push(4999, 2);
    sparse_set.push(5000, 3);

    for(u16 v : sparse_set) {
        std::cout << v << std::endl;
    }
    

    return 0;
}