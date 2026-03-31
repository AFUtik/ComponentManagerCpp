#pragma once

#include <cstdint>
#include <iostream>

#include "../EntityManager.hpp"
#include "../EventManager.hpp"

using u64 = std::uint64_t;

struct BasicComponent : public EntityComponent<BasicComponent> {
    void init() override {
        std::cout << "Component Init" << std::endl;
    }

    void drop() override {
        std::cout << "Component Drop" << std::endl;   
    }

    void on_event(const BasicEvent &event) {
        std::cout << "Component: " << event.data << std::endl;
    }
};