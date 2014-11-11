#include "stdafx.h"
#include <stdio.h>
#include <math.h>
#include <Mmsystem.h>
#include <d2d1.h>
#include <Wincodec.h>

#include "d3dutil.h"
#include "xnamath.h"

void SaveImageToBMP(char* szFileName, BYTE* pData, int Width, int Height)      //fortest [lijunsi0413]
{
  unsigned long ImageSize;	//图像的大小
  
  ImageSize = Width*Height * 4;
  
  //9 初始化BMP文件头
  BITMAPFILEHEADER bmpfileheader = { 0 };
  BITMAPINFOHEADER bih = { 0 };
  bih.biBitCount = 32;
  bih.biSize = sizeof(BITMAPINFOHEADER);
  bih.biWidth = Width;
  bih.biHeight = Height;
  bih.biPlanes = 1;
  bih.biSizeImage = ImageSize;
  bih.biCompression = BI_RGB;

  bmpfileheader.bfType = 0x4d42;
  bmpfileheader.bfSize = sizeof(bmpfileheader)+sizeof(bih)+ImageSize;
  bmpfileheader.bfOffBits = sizeof(bmpfileheader)+sizeof(bih);

  FILE* pfile = NULL;
  pfile = fopen(szFileName, "wb");

  int wsize = fwrite(&bmpfileheader, sizeof(bmpfileheader), 1, pfile);
  fwrite(&bih, sizeof(bih), 1, pfile);
  fwrite(pData, ImageSize, 1, pfile);
  fflush(pfile);
  fclose(pfile);
}


// GaussianBlur
struct VECTOR2
{
  float x_;
  float y_;
  VECTOR2(float x, float y)
    : x_(x)
    , y_(y) {
  }
  VECTOR2() {}
  VECTOR2& operator =(const VECTOR2& rhs)
  {
    this->x_ = rhs.x_;
    this->y_ = rhs.y_;
    return *this;
  }
};

static VECTOR2* g_TemplateOffset = NULL;
static float* g_TemplateWeight = NULL;

// 计算权重因子
float GetGaussianDistribution(float x, float y, float deviation)
{
  //  deviation += EPSILON;
  float g = 1.0 / (2 * M_PI * deviation * deviation);

  return g * exp(-(x * x + y * y) / (2 * deviation * deviation));
}

static int textureWidth = 128;

// 生成高斯模板
void GenerateGaussianTemplate(VECTOR2* templateOffset, float* templateWeight, int radius, float deviation)
{
  VECTOR2 center = { 0.0f, 0.0f };
  templateOffset[0] = center;
  templateWeight[0] = GetGaussianDistribution(0.0, 0.0, deviation);
  int EdgeSize = radius * 2 + 1;
  int Size = EdgeSize * EdgeSize;
  float deltaX = 1.0 / (textureWidth * 1.0);
  float deltaY = 1.0 / (textureWidth * 1.0);
  int OffsetIndex = 1;

  for (int i = 0; i < EdgeSize; i++)
  {
    for (int j = 0; j < EdgeSize; j++)
    {
      int OffsetXBase = 0;
      int OffsetYBase = 0;
      if (i == radius)
      {
        OffsetYBase = 0;
      }
      else
      {
        OffsetYBase = i - radius;
      }

      if (j == radius)
      {
        OffsetXBase = 0;

      }
      else
      {
        OffsetXBase = j - radius;
      }
      if (OffsetXBase == 0 && OffsetYBase == 0)
        continue;

      VECTOR2 delta = { OffsetXBase * deltaX, OffsetYBase * deltaY };
      templateOffset[OffsetIndex] = delta;
      VECTOR2 gDelta;
      gDelta.x_ = OffsetXBase * 1.0;
      gDelta.y_ = OffsetYBase * 1.0;
      float g = GetGaussianDistribution(gDelta.x_, gDelta.y_, deviation);
      templateWeight[OffsetIndex] = g;
      OffsetIndex++;
    }
  }

  float total = 0.0;
  int i = 0, j = 0;
  for (/*int i = 0*/; i < Size; i++)
  {
    total += templateWeight[i];
  }
  for (/*int i = 0*/; j < Size; j++)
  {
    templateWeight[j] = templateWeight[j] / total;
  }

  return;
}

//--------------------------------------------------------------------------------------
// Structures
//--------------------------------------------------------------------------------------
struct SimpleVertex
{
  XMFLOAT3 Pos;
  XMFLOAT2 Tex;
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
ID3D11Buffer* g_vertexIndexBuffer = NULL;
ID3D11ShaderResourceView* g_ShaderResourceView = NULL;
ID3D11SamplerState* g_SamplerState = NULL;
ID3D11Buffer* g_SampleWeightsBuffer = NULL;
ID3D11Buffer* g_SampleOffsetsBuffer = NULL;
ID3D11Buffer* g_Radius = NULL;
ID3D11Texture2D* g_Texture = NULL;
ID3D11ShaderResourceView* g_ShaderResourceViewFromBitmap = NULL;

IWICImagingFactory* g_WICImageFactary = NULL;
IWICBitmapDecoder* g_BitmapDecoder = NULL;
IWICBitmapFrameDecode* g_BitmapFrameDecode = NULL;
IWICBitmapScaler* g_BitmapScalar = NULL;

namespace d3d
{

  HRESULT InitD3D11(HINSTANCE hInstance, HWND hwnd, int width, int height, bool windowed)
  {
    HRESULT hr = S_OK;
    
    // fortest
    width = 621;
    height = 1104;

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
      { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
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
      { XMFLOAT3(-0.621f, 1.104f, 0.9f), XMFLOAT2(0.0f, 0.0f) },
      { XMFLOAT3(0.621f, 1.104f, 0.9f), XMFLOAT2(1.0f, 0.0f) },
      { XMFLOAT3(-0.621f, -1.104f, 0.9f), XMFLOAT2(0.0f, 1.0f) },
      { XMFLOAT3(0.621f, -1.104f, 0.9f), XMFLOAT2(1.0f, 1.0f) },
    };

    D3D11_BUFFER_DESC bd;
    ZeroMemory(&bd, sizeof(D3D11_BUFFER_DESC));
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(SimpleVertex)* (sizeof(vertices) / sizeof(vertices[0]));
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

    // Create Indices buffer
    WORD indices[] =
    {
      0, 1, 2,
      1, 3, 2,
    };

    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(WORD)* ARRAYSIZE(indices);
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    bd.CPUAccessFlags = 0;
    InitData.pSysMem = indices;
    hr = g_D3D11Device->CreateBuffer(&bd, &InitData, &g_vertexIndexBuffer);
    if (FAILED(hr))
      return hr;

    g_deviceContext->IASetIndexBuffer(g_vertexIndexBuffer, DXGI_FORMAT_R16_UINT, 0);

    // 创建WIC
    hr = CoCreateInstance(CLSID_WICImagingFactory,
      NULL,
      CLSCTX_INPROC_SERVER,
      IID_IWICImagingFactory,
      reinterpret_cast<void**>(&g_WICImageFactary));
    if (FAILED(hr))
    {
      ::MessageBox(NULL, L"WICFactory Failed", NULL, NULL);
    }

    hr = g_WICImageFactary->CreateDecoderFromFilename(L"QQ图片20141022120540.jpg",
      NULL,
      GENERIC_READ,
      WICDecodeMetadataCacheOnDemand,
      &g_BitmapDecoder);
    if (FAILED(hr))
    {
      ::MessageBox(NULL, L"CreateDecoderFromFilename Failed", NULL, NULL);
    }

    hr = g_BitmapDecoder->GetFrame(0, &g_BitmapFrameDecode);

    UINT bitWidth = 0, bitHeight = 0;
    g_BitmapFrameDecode->GetSize(&bitWidth, &bitHeight);
    UINT scaleWidth = bitWidth / 16;
    UINT scaleHeight = bitHeight / 16;

    IWICFormatConverter *pConverter = nullptr;
    hr = g_WICImageFactary->CreateFormatConverter(&pConverter);

    // 从jpg转成bmp
    hr = pConverter->Initialize(g_BitmapFrameDecode, /*GUID_WICPixelFormat32bppPBGRA*/GUID_WICPixelFormat32bppPRGBA,
      WICBitmapDitherTypeNone,
      NULL,
      0.f,
      WICBitmapPaletteTypeMedianCut);

    //BYTE* converterBuffer = new BYTE[bitWidth * bitHeight * 4];
    //ZeroMemory(converterBuffer, sizeof(bitWidth * bitHeight * 4));
    //pConverter->CopyPixels(NULL, bitWidth * 4, bitWidth * bitHeight * 4, converterBuffer);
    //SaveImageToBMP("converter.bmp", converterBuffer, bitWidth, bitHeight);

    /*WICPixelFormatGUID pixelFormatGUID;
    g_BitmapFrameDecode->GetPixelFormat(&pixelFormatGUID);

    BYTE* imageBufferOrigin = new BYTE[bitWidth * bitHeight * 4];
    ZeroMemory(imageBufferOrigin, bitWidth * bitHeight * 4);
    g_BitmapFrameDecode->CopyPixels(NULL, bitWidth * 4, bitHeight * bitWidth * 4, imageBufferOrigin);*/

    //IWICBitmap* pWicBitmap = NULL;
    //IWICBitmapLock* pBitmapLock = NULL;
    //hr = g_WICImageFactary->CreateBitmap(bitWidth, bitHeight, pixelFormatGUID, WICBitmapCacheOnDemand, &pWicBitmap);
    //pWicBitmap->Lock(NULL, WICBitmapLockRead, &pBitmapLock);
    //UINT strided = 0;
    //UINT imagesize = 0;
    //BYTE* pv = NULL;
    //pBitmapLock->GetStride(&stride);
    //pBitmapLock->GetDataPointer(&imagesize, &pv);
    //// fortest
    //SaveImageToBMP("captureorigin.bmp", imageBufferOrigin, bitWidth, bitHeight);

    hr = g_WICImageFactary->CreateBitmapScaler(&g_BitmapScalar);
    if (FAILED(hr))
    {
      ::MessageBox(NULL, L"CreateBitmapScaler Failed", NULL, NULL);
    }

    // 放缩图片 WICBitmapInterpolationModeNearestNeighbor
    hr = g_BitmapScalar->Initialize(pConverter, scaleWidth, scaleHeight, /*WICBitmapInterpolationModeLinear*/WICBitmapInterpolationModeFant);
    //

    // 获得放缩后的数据
    BYTE* imageBuffer = new BYTE[scaleWidth * scaleHeight * 4];
    hr = g_BitmapScalar->CopyPixels(NULL, scaleWidth * 4, scaleWidth * scaleHeight * 4, imageBuffer);

    // fortest
    SaveImageToBMP("capture.bmp", imageBuffer, scaleWidth, scaleHeight);

    //// 再放大
    //IWICBitmapScaler* plargeScalar = NULL;
    //hr = g_WICImageFactary->CreateBitmapScaler(&plargeScalar);
    //plargeScalar->Initialize(g_BitmapScalar, bitWidth, bitHeight, WICBitmapInterpolationModeLinear);
    //BYTE* imageBufferLarge = new BYTE[bitWidth * bitHeight * 4];
    //hr = plargeScalar->CopyPixels(NULL, bitWidth * 4, bitWidth * bitHeight * 4, imageBufferLarge);
    //// fortest
    //SaveImageToBMP("capturelarge.bmp", imageBufferLarge, bitWidth, bitHeight);

    // 将缩小后数据放入纹理
    D3D11_TEXTURE2D_DESC texture2dDesc;
    ZeroMemory(&texture2dDesc, sizeof(D3D11_TEXTURE2D_DESC));
    texture2dDesc.Width = scaleWidth;
    texture2dDesc.Height = scaleHeight;
    texture2dDesc.ArraySize = 1;
    texture2dDesc.MipLevels = 1;
    texture2dDesc.Usage = D3D11_USAGE_DEFAULT;
    texture2dDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    texture2dDesc.CPUAccessFlags = 0;
    texture2dDesc.Format = /*DXGI_FORMAT_R8G8B8A8_UNORM*/DXGI_FORMAT_B8G8R8A8_UNORM;
    
    texture2dDesc.SampleDesc.Quality = 0;
    texture2dDesc.SampleDesc.Count = 1;

    D3D11_SUBRESOURCE_DATA subresourceData;
    ZeroMemory(&subresourceData, sizeof(D3D11_SUBRESOURCE_DATA));
    subresourceData.pSysMem = (void *)imageBuffer;
    subresourceData.SysMemPitch = scaleWidth * 4;
    subresourceData.SysMemSlicePitch = scaleWidth * scaleHeight * 4;

    hr = g_D3D11Device->CreateTexture2D(&texture2dDesc, &subresourceData, &g_Texture);

    delete[] imageBuffer;

    // 由bitmap texture2d创建shaderresourceview
    hr = g_D3D11Device->CreateShaderResourceView(g_Texture, NULL, &g_ShaderResourceViewFromBitmap);
    D3D11_SHADER_RESOURCE_VIEW_DESC textureShaderDesc;
    g_ShaderResourceViewFromBitmap->GetDesc(&textureShaderDesc);

    // 创建纹理，载入纹理 /*"QQ图片20141022120540.jpg"*/ "leopard.jpg"
    hr = D3DX11CreateShaderResourceViewFromFile(g_D3D11Device, L"QQ图片20141022120540.jpg", NULL, NULL, &g_ShaderResourceView, NULL);
    if (FAILED(hr))
      return hr;

    D3D11_SHADER_RESOURCE_VIEW_DESC shaderDesc;
    g_ShaderResourceView->GetDesc(&shaderDesc);

    // 创建采样器
    D3D11_SAMPLER_DESC sampDesc;
    ZeroMemory(&sampDesc, sizeof(sampDesc));
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    hr = g_D3D11Device->CreateSamplerState(&sampDesc, &g_SamplerState);
    if (FAILED(hr))
      return hr;

    // Set primitive topology
    g_deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 创建高斯模板
    float deviation = 1.0;
    int radius = ceil(deviation * 3);
    int diameter = radius * 2 + 1;
    int size = diameter * diameter;
    g_TemplateOffset = new VECTOR2[size];
    g_TemplateWeight = new float[size];

    GenerateGaussianTemplate(g_TemplateOffset, g_TemplateWeight, radius, deviation);

    VECTOR2 templateOffset[49];
    float templateWeight[49];
    for (size_t i = 0; i < 49; i++)
    {
      templateOffset[i] = g_TemplateOffset[i];
      templateWeight[i] = g_TemplateWeight[i];
    }
    double totalColor = 0.0;
    for (size_t i = 0; i < 49; i++)
    {
      totalColor += templateWeight[i];
    }
    //

    D3D11_BUFFER_DESC bufferDesc;
    ZeroMemory(&bufferDesc, sizeof(bufferDesc));
    bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    bufferDesc.ByteWidth = ceil(sizeof(float)* size / 16.0) * 16;
    bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    D3D11_SUBRESOURCE_DATA SubInitData;
    ZeroMemory(&SubInitData, sizeof(SubInitData));
    SubInitData.pSysMem = g_TemplateWeight;
    hr = g_D3D11Device->CreateBuffer(&bufferDesc, &SubInitData, &g_SampleWeightsBuffer);
    if (FAILED(hr))
    {
      Release(g_SampleWeightsBuffer);
      return hr;
    }

    bufferDesc.ByteWidth = ceil(sizeof(VECTOR2)* size / 16.0) * 16;
    SubInitData.pSysMem = g_TemplateOffset;
    hr = g_D3D11Device->CreateBuffer(&bufferDesc, &SubInitData, &g_SampleOffsetsBuffer);
    if (FAILED(hr))
    {
      Release(g_SampleOffsetsBuffer);
      return hr;
    }

    bufferDesc.ByteWidth = ceil(sizeof(UINT) / 16.0) * 16;
    SubInitData.pSysMem = &radius;
    hr = g_D3D11Device->CreateBuffer(&bufferDesc, &SubInitData, &g_Radius);
    if (FAILED(hr))
    {
      Release(g_Radius);
      return hr;
    }

    g_deviceContext->PSSetConstantBuffers(0, 1, &g_SampleWeightsBuffer);
    g_deviceContext->PSSetConstantBuffers(1, 1, &g_SampleOffsetsBuffer);
    g_deviceContext->PSSetConstantBuffers(2, 1, &g_Radius);

    return S_OK;
  }

  void Render(float deltaTime)
  {
    float ClearColor[4] = { 0.0f, 0.125f, 0.3f, 1.0f };
    g_deviceContext->ClearRenderTargetView(g_RTView, ClearColor);

    // render a triangle
    g_deviceContext->VSSetShader(g_pVSShader, NULL, 0);
    g_deviceContext->PSSetShader(g_pPSShader, NULL, 0);
    //g_deviceContext->PSSetShaderResources(0, 1, &g_ShaderResourceView);
    g_deviceContext->PSSetShaderResources(0, 1, &g_ShaderResourceViewFromBitmap);
    g_deviceContext->PSSetSamplers(0, 1, &g_SamplerState);

    //g_deviceContext->UpdateSubresource(g_SampleOffsetsBuffer, 0, NULL, g_TemplateOffset, 0, 0);
    //g_deviceContext->UpdateSubresource(g_SampleWeightsBuffer, 0, NULL, g_TemplateWeight, 0, 0);
    g_deviceContext->DrawIndexed(6, 0, 0);

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

    ID3D11Buffer* g_SampleWeightsBuffer = NULL;
    ID3D11Buffer* g_SampleOffsetsBuffer = NULL;
    ID3D11Buffer* g_Radius = NULL;

    if (g_SampleOffsetsBuffer) {
      Release(g_SampleOffsetsBuffer);
    }

    if (g_SampleWeightsBuffer)
    {
      Release(g_SampleWeightsBuffer);
    }

    if (g_Radius)
    {
      Release(g_Radius);
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
      else
      {
        float currentTime = (float)timeGetTime();
        float deltaTime = (currentTime - lastTime) * 0.001f;
        if (deltaTime < 0.03f) {
          Sleep(50);
          continue;
        }
        lastTime = currentTime;
        ptr_display(deltaTime);
      }
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