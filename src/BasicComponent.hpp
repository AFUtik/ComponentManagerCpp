#pragma once

#include "EntityManager.hpp"

struct BasicComponent : public ECS::Component<BasicComponent> {};
struct BasicComponent2 : public ECS::Component<BasicComponent> {};