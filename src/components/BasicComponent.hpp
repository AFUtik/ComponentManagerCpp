#pragma once

#include <cstdint>
#include <iostream>

#include "../EntityManager.hpp"
#include "../EventManager.hpp"

using u64 = std::uint64_t;

struct BasicComponent : public EntityComponent<BasicComponent> {
    u64 number = 0;

    BasicComponent(u64 num) : number(num) {}

    void print() {
        std::cout << number << std::endl;
    }
};