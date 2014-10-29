#include "stdafx.h"
#include <Mmsystem.h>

#include "d3dutil.h"

namespace d3d
{
  HWND g_Hwnd = NULL;
  IDXGISwapChain* g_SwapChain = NULL;
  ID3D11Device* g_D3D11Device = NULL;
  D3D_FEATURE_LEVEL g_featureLevel;
  ID3D11DeviceContext* g_deviceContext = NULL;
  ID3D11RenderTargetView* g_RTView = NULL;

  bool InitD3D11(HINSTANCE hInstance, HWND hwnd, int width, int height, bool windowed)
  {
    HRESULT hr = S_OK;
    
    g_Hwnd = hwnd;

    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_DRIVER_TYPE driverTypes[] =
    {
      D3D_DRIVER_TYPE_HARDWARE,
      D3D_DRIVER_TYPE_WARP,
      D3D_DRIVER_TYPE_REFERENCE,
    };
    UINT numDriverTypes = sizeof(driverTypes) / sizeof(driverTypes[0]);

    D3D_FEATURE_LEVEL featureLevel[] =
    {
      D3D_FEATURE_LEVEL_11_0,
      D3D_FEATURE_LEVEL_10_0,
      D3D_FEATURE_LEVEL_10_1,
    };
    UINT numFeatureLevels = ARRAYSIZE(featureLevel);

    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 1;
    sd.BufferDesc.Width = width;
    sd.BufferDesc.Height = height;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = windowed;

    for (UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++)
    {
      hr = D3D11CreateDeviceAndSwapChain(NULL, driverTypes[driverTypeIndex], NULL, createDeviceFlags, featureLevel, numFeatureLevels,
        D3D11_SDK_VERSION, &sd, &g_SwapChain, &g_D3D11Device, &g_featureLevel, &g_deviceContext);
      if (SUCCEEDED(hr))
        break;
    }
    if (FAILED(hr))
      return false;

    // Create a render target view
    ID3D11Texture2D* pBackBuffer = NULL;
    hr = g_SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&pBackBuffer));
    if (FAILED(hr))
      return false;

    hr = g_D3D11Device->CreateRenderTargetView(pBackBuffer, NULL, &g_RTView);
    Release(pBackBuffer);
    if (FAILED(hr)) {
      return false;
    }

    g_deviceContext->OMSetRenderTargets(1, &g_RTView, NULL);

    // Setup the view port
    D3D11_VIEWPORT vp;
    vp.Width = (FLOAT)width;
    vp.Height = (FLOAT)height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    g_deviceContext->RSSetViewports(1, &vp);

    // Change Window Proc
    SetWindowLongPtr(hwnd, GWL_WNDPROC, (LONG)WndProc);

    return true;
  }

  void Render(float deltaTime)
  {
    float ClearColor[4] = { 0.0f, 0.125f, 0.3f, 1.0f };
    g_deviceContext->ClearRenderTargetView(g_RTView, ClearColor);
    g_SwapChain->Present(0, 0);
  }

  void CleanupDevice()
  {
    if (g_deviceContext)
      g_deviceContext->ClearState();
    if (g_RTView)
    {
      Release(g_RTView);
    }

    if (g_SwapChain) {
      Release(g_SwapChain);
    }

    if (g_deviceContext) {
      Release(g_deviceContext);
    }

    if (g_D3D11Device)
      Release(g_D3D11Device);
  }

  LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
  {
    int wmId, wmEvent;
    PAINTSTRUCT ps;
    HDC hdc;

    switch (msg)
    {
    case WM_COMMAND:
      wmId = LOWORD(wParam);
      wmEvent = HIWORD(wParam);
      break;
    case WM_PAINT:
      hdc = BeginPaint(hWnd, &ps);
      // TODO: Add any drawing code here...
      EndPaint(hWnd, &ps);
      break;
    case WM_DESTROY:
      PostQuitMessage(0);
      break;
    case WM_KEYDOWN:
      if (wParam == VK_ESCAPE)
        ::DestroyWindow(g_Hwnd);
      break;
    default:
      return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
  }

  void EnterMsgLoop(void(*ptr_display)(float timeDelta))
  {
    MSG msg;
    ZeroMemory(&msg, sizeof(msg));
    static float lastTime = (FLOAT)timeGetTime();
    while (msg.message != WM_QUIT)
    {
      if (PeekMessage(&msg, NULL, NULL, NULL, PM_REMOVE))
      {
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
      }

      float currentTime = (float)timeGetTime();
      float deltaTime = (currentTime - lastTime) * 0.001f;
      if (deltaTime < 1.0f) {
        continue;
      }
      ptr_display(deltaTime);
    }
  }

} // namespace d3d