#define IMGUI_DEFINE_MATH_OPERATORS
#pragma once

// Prevenir redefinição de winsock
#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#endif

// Windows Libraries
#include <Windows.h>
#include <iostream>
#include <dwmapi.h>
#include <TlHelp32.h>

// Render
#include "../src/Interface.hpp"
#include "../src/Overlay.hpp"
#include "../src/Assets.hpp"
#include "../resource/Assets/FontAwesome.hpp"
#include "../resource/Assets/FontInter.hpp"

// ImGui
#include "incluides/ImGui/imgui.h"
#include "incluides/ImGui/imgui_impl_dx11.h"
#include "incluides/ImGui/imgui_impl_win32.h"
#include "incluides/ImGui/imgui_internal.h"


// DirectX Inlcude
#include <d3d11.h>
#include <d3dx11.h>

// Random
#include <chrono>