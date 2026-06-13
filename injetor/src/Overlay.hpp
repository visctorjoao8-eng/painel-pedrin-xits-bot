#pragma once

#include <Windows.h>
#include <functional>

#include <d3d11.h>
#include <d3dx11.h>

#include <../resource/FrameWork.hpp>

namespace FrameWork
{
    class Condif
    {
    public:
        void Initialize();
        void Mouse_Move();
    };

    inline Condif Overlay;
}
