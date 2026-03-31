#pragma once

#include <cstdint>
#include <iostream>

#include "../EntityManager.hpp"
#include "../EventManager.hpp"

using u64 = std::uint64_t;

struct BasicComponent : public EntityComponent<BasicComponent> {
    EventManager::Subscription<BasicComponent, u32, BasicEvent> sub;

    void init() override {
        sub.add<BasicEvent, &BasicComponent::on_event>(this);
    }

    void on_event(const BasicEvent &event) {
        std::cout << "Component: " << event.data << std::endl;
    }
};