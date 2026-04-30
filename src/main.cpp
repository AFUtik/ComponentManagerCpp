#include <iostream>

#include "ComponentManager.hpp"

struct ECS : public ComponentManager<ECS, u64, Empty, 64, 256>
{ 
    static ECS& instance()
    { 
        static ECS ecs;
        return ecs;
    }
};

using Entity = ECS::Object;

template<typename T>
using EntityComponent = ECS::Component<T>;

template<typename T>
using PlainEntityComponent = ECS::PlainComponent<T>;

struct Vec2
{
    double x;
    double y;
};

struct ECPosition : public PlainEntityComponent<ECPosition> 
{
    Vec2 position;
};

struct ECAcceleration : public PlainEntityComponent<ECAcceleration>
{
    Vec2 acceleration;

    ECAcceleration(Vec2 acc) : acceleration(acc) {}
};

struct ECVelocity : public PlainEntityComponent<ECVelocity>
{
    Vec2 velocity;

    ECVelocity(Vec2 vel) : velocity(vel) {}
};

struct ECTag : public PlainEntityComponent<ECTag>
{ 
    std::string tag;

    ECTag(ECTag&&) noexcept = default;

    ECTag(const std::string s) : tag(s) {
        std::cout << "Constructor has been invoked" << std::endl;
    }

    ~ECTag() {
        std::cout << "Destructor has been invoked" << std::endl;
    }
};

int main(int, char**)
{
    auto& manager = ECS::instance();
    manager.register_type<ECPosition>();
    manager.register_type<ECAcceleration>();
    manager.register_type<ECVelocity>();
    manager.register_type<ECTag>();

    Entity ent = manager.create_object();
    manager.template add_component<ECTag>(ent, "Some tag");
    manager.template add_component<ECVelocity>(ent, ECVelocity{ Vec2(50.0, 50.0) });

    ECS::View<ECTag, ECVelocity> view = ECS::View<ECTag, ECVelocity>(&manager);
    for(auto [tag, vel] : view)
    {
        std::cout << vel.velocity.x << std::endl;
    }

    std::cout << sizeof(manager) << std::endl;

    return 0;
}