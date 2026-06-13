#pragma once

#include <../resource/FrameWork.hpp>
#include <d3d11.h>
#include <d3dx11.h>

namespace FrameWork
{
    class Interface
    {
    public:
        void UpdateStyle();
        void RenderGui();
    private:
        char DiscordId[255] = "";
        int CurrentTab = 0;
    };

    inline Interface CoInterface;
}
