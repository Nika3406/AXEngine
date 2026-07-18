#pragma once

class Entity; // forward

class Component {
public:
    virtual ~Component() = default;

    virtual void onStart()  {}
    virtual void onUpdate(float dt) {}

    Entity* owner = nullptr;
};
