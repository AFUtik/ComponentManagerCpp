#include <iostream>
#include <limits>

#include "ComponentManager.hpp"
#include "QuadTree.hpp"

struct ECS : public ComponentManager<ECS, u64, Empty, 1024, 64, 256>
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

using QuadTree = QuadTree_Static<
    u32, f64,
    Vec2, 
    u16, u16, 
    1<<4, 1<<8, 16, 8
>;

int main(int, char**)
{
    const f64 f64_min = std::numeric_limits<f64>::min();
    const f64 f64_max = std::numeric_limits<f64>::max();

    QuadTree tree(
        QuadTree::AABB{f64_min, f64_min, f64_max, f64_max}
    );

    std::cout << sizeof(QuadTree::Node) << std::endl;

    return 0;
}