#include "Overlay.hpp"

// Definição global — usada em Security.cpp e main.cpp via extern
bool g_ForceKeepRunning = false;

RECT rc = { 0 };
HWND hwnd;
ID3D11Device* ID3dDevice;
ID3D11DeviceContext* ID3dDeviceContext;
IDXGISwapChain* ID3dSwapChain;
ID3D11RenderTargetView* ID3dRenderTargetView;
static UINT g_ResizeWidth = 0, g_ResizeHeight = 0;

bool CreateDeviceD3D(HWND hWnd);
void CreateRenderTarget();
void CleanupDeviceD3D();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace FrameWork
{
    void Condif::Initialize()
    {
        // Carregar ícone padrão do Windows
        HICON hIcon = LoadIcon(NULL, IDI_APPLICATION);
  
        WNDCLASSEX WndClass = { 
            sizeof(WNDCLASSEX), 
            CS_CLASSDC, 
            WndProc, 
            0L, 0L, 
            GetModuleHandle(NULL), 
            hIcon,              // Ícone da janela
            NULL, 
            NULL, 
            NULL, 
            L"SyncStore", 
            hIcon               // Ícone pequeno
        };
        RegisterClassEx(&WndClass);
        
        // Criar janela com WS_EX_APPWINDOW para aparecer na barra de tarefas
        hwnd = CreateWindowEx(
            WS_EX_APPWINDOW | WS_EX_LAYERED, 
            WndClass.lpszClassName, 
            L"Pedrin Xits", 
            WS_POPUP, 
            (GetSystemMetrics(SM_CXSCREEN) / 2) - (565 / 2), 
            (GetSystemMetrics(SM_CYSCREEN) / 2) - (385 / 2), 
            565, 385, 
            NULL, NULL, 
            WndClass.hInstance, 
            NULL
        );

        SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 255, LWA_ALPHA);

        MARGINS margins = { -1 };
        DwmExtendFrameIntoClientArea(hwnd, &margins);

        RECT rc = { 0 };
        GetWindowRect(hwnd, &rc);

        if (!CreateDeviceD3D(hwnd))
        {
            CleanupDeviceD3D();
            UnregisterClass(WndClass.lpszClassName, WndClass.hInstance);
            return;
        }

        ShowWindow(hwnd, SW_SHOWDEFAULT);
        UpdateWindow(hwnd);

        ImGui::CreateContext();
        CoInterface.UpdateStyle();
        Assets::Initialize(ID3dDevice);

        ImGui_ImplWin32_Init(hwnd);
        ImGui_ImplDX11_Init(ID3dDevice, ID3dDeviceContext);

        bool done = false;
        while (!done)
        {
            MSG msg;
            while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
            {
                ::TranslateMessage(&msg);
                ::DispatchMessage(&msg);
                if (msg.message == WM_QUIT)
                    done = true;
            }
            if (done)
                break;

            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();
            CoInterface.RenderGui();
            ImGui::Render();


            float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            ID3dDeviceContext->OMSetRenderTargets(1, &ID3dRenderTargetView, NULL);
            ID3dDeviceContext->ClearRenderTargetView(ID3dRenderTargetView, clearColor);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

            ID3dSwapChain->Present(1, 0);
        }

        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        CleanupDeviceD3D();
        DestroyWindow(hwnd);
        UnregisterClass(WndClass.lpszClassName, WndClass.hInstance);
    }
    void Condif::Mouse_Move() {

        ImGui::SetCursorPos(ImVec2(0, 0));
        if (ImGui::InvisibleButton(("Move_detector"), ImVec2(565, 385)));
        if (ImGui::IsItemActive())
        {
            GetWindowRect(hwnd, &rc);
            MoveWindow(hwnd, rc.left + ImGui::GetMouseDragDelta().x, rc.top + ImGui::GetMouseDragDelta().y, 565, 385, TRUE);
        }
    }

}

bool CreateDeviceD3D(HWND hWnd)
{
    // Setup swap chain
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 75;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &ID3dSwapChain, &ID3dDevice, &featureLevel, &ID3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED)
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &ID3dSwapChain, &ID3dDevice, &featureLevel, &ID3dDeviceContext);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (ID3dSwapChain) { ID3dSwapChain->Release(); ID3dSwapChain = nullptr; }
    if (ID3dDeviceContext) { ID3dDeviceContext->Release(); ID3dDeviceContext = nullptr; }
    if (ID3dDevice) { ID3dDevice->Release(); ID3dDevice = nullptr; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    ID3dSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    ID3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &ID3dRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (ID3dRenderTargetView) { ID3dRenderTargetView->Release(); ID3dRenderTargetView = nullptr; }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam);
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
