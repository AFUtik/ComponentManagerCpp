#include <iostream>
#include "BasicComponent.hpp"

int main(int, char**)
{
    ECS manager;
    manager.register_type<BasicComponent>();
    manager.register_type<BasicComponent2>();

    std::cout << ECS::Component<BasicComponent2>::_id << std::endl; 

    return 0;
}