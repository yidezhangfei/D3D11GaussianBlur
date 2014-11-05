#include "stdafx.h"
#include <Mmsystem.h>

#include "d3dutil.h"
#include "xnamath.h"

//--------------------------------------------------------------------------------------
// Structures
//--------------------------------------------------------------------------------------
struct SimpleVertex
{
  XMFLOAT3 Pos;
};

HWND g_Hwnd = NULL;
IDXGISwapChain* g_SwapChain = NULL;
ID3D11Device* g_D3D11Device = NULL;
D3D_FEATURE_LEVEL g_featureLevel;
ID3D11DeviceContext* g_deviceContext = NULL;
ID3D11RenderTargetView* g_RTView = NULL;
ID3D11VertexShader* g_pVSShader = NULL;
ID3D11InputLayout* g_pInputLayout = NULL;
ID3D11PixelShader* g_pPSShader = NULL;
ID3D11Buffer* g_vertexBuffer = NULL;

namespace d3d
{

  HRESULT InitD3D11(HINSTANCE hInstance, HWND hwnd, int width, int height, bool windowed)
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

    // Compile the vertex shader.
    ID3DBlob* pVSBlob = NULL;
    hr = CompileShaderFromFile(L"effect.fx", "VS", "vs_4_0", &pVSBlob);
    if (FAILED(hr))
    {
      MessageBox(NULL, L"The fx file can't be compiled.", NULL, NULL);
      return hr;
    }

    // Create the vertex shader
    hr = g_D3D11Device->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), NULL,
      &g_pVSShader);
    if (FAILED(hr))
    {
      Release(pVSBlob);
      return hr;
    }

    // Define the input layout
    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
      { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    UINT numElements = ARRAYSIZE(layout);

    // Create the input layout
    hr = g_D3D11Device->CreateInputLayout(layout, numElements, pVSBlob->GetBufferPointer(),
      pVSBlob->GetBufferSize(), &g_pInputLayout);
    Release(pVSBlob);
    if (FAILED(hr))
    {
      return hr;
    }

    // Set the input layout
    g_deviceContext->IASetInputLayout(g_pInputLayout);

    // Compile the ps shader
    ID3DBlob* pPSBlob = NULL;
    hr = CompileShaderFromFile(L"effect.fx", "PS", "ps_4_0", &pPSBlob);
    if (FAILED(hr))
    {
      MessageBox(NULL, L"the fx file can't be compiled", NULL, NULL);
      return hr;
    }

    // Create the pixel shader
    hr = g_D3D11Device->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(),
      NULL, &g_pPSShader);
    Release(pPSBlob);
    if (FAILED(hr))
      return hr;

    // Create vertex buffer
    SimpleVertex vertices[] =
    {
      XMFLOAT3(0.0f, 0.5f, 0.5f),
      XMFLOAT3(0.5f, -0.5f, 0.5f),
      XMFLOAT3(-0.5f, -0.5f, 0.5f),
    };

    D3D11_BUFFER_DESC bd;
    ZeroMemory(&bd, sizeof(D3D11_BUFFER_DESC));
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(SimpleVertex)* 3;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = 0;
    D3D11_SUBRESOURCE_DATA InitData;
    ZeroMemory(&InitData, sizeof(InitData));
    InitData.pSysMem = vertices;
    hr = g_D3D11Device->CreateBuffer(&bd, &InitData, &g_vertexBuffer);
    if (FAILED(hr))
      return hr;

    // Set Vertex buffer
    UINT stride = sizeof(SimpleVertex);
    UINT offset = 0;
    g_deviceContext->IASetVertexBuffers(0, 1, &g_vertexBuffer, &stride, &offset);

    // Set primitive topology
    g_deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    return S_OK;
  }

  void Render(float deltaTime)
  {
    float ClearColor[4] = { 0.0f, 0.125f, 0.3f, 1.0f };
    g_deviceContext->ClearRenderTargetView(g_RTView, ClearColor);

    // render a triangle
    g_deviceContext->VSSetShader(g_pVSShader, NULL, 0);
    g_deviceContext->PSSetShader(g_pPSShader, NULL, 0);
    g_deviceContext->Draw(3, 0);

    g_SwapChain->Present(0, 0);
  }

  void CleanupDevice()
  {
    if (g_deviceContext)
      g_deviceContext->ClearState();

    if (g_pVSShader)
    {
      Release(g_pVSShader);
      g_pVSShader = NULL;
    }

    if (g_pPSShader)
    {
      Release(g_pPSShader);
      g_pPSShader = NULL;
    }

    if (g_pInputLayout)
    {
      Release(g_pInputLayout);
      g_pInputLayout = NULL;
    }

    if (g_vertexBuffer)
    {
      Release(g_vertexBuffer);
      g_vertexBuffer = NULL;
    }

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

  HRESULT CompileShaderFromFile(wchar_t* szFileName, LPCSTR szEntryPoint,
    LPCSTR szShaderModel, ID3DBlob** ppBlobOut)
  {
    HRESULT hr = S_OK;

    DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined ( _DEBUG )
    // Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
    dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif

    ID3DBlob* pErrorBlob;
    hr = D3DX11CompileFromFile(szFileName, NULL, NULL, szEntryPoint, szShaderModel,
      dwShaderFlags, 0, NULL, ppBlobOut, &pErrorBlob, NULL);
    if (FAILED(hr))
    {
      if (pErrorBlob != NULL)
        OutputDebugStringA((char*)pErrorBlob->GetBufferPointer());
      if (pErrorBlob)
        pErrorBlob->Release();
      return hr;
    }

    if (pErrorBlob)
      pErrorBlob->Release();

    return S_OK;
  }

} // namespace d3d