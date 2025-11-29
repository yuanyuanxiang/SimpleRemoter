
#pragma once

class App
{
public:
    App() {}
    virtual ~App() {}

    virtual bool Initialize() = 0;
    virtual bool Start(bool block) = 0;
    virtual bool Stop() = 0;
};
