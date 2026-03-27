#include "ComponentManager.hpp"

struct EntityObj {
    
};

struct ECS : ComponentManager<
    ECS, 
    u32, 
    EntityObj, 
    256, 
    256>
{
    ECS() : ComponentManager() {}
};