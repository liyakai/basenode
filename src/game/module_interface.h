#pragma once


class IModule
{
public:
    virtual ~IModule() = default;
    virtual void Init() = 0;
    virtual void Update() = 0;
    virtual void UnInit() = 0;
};