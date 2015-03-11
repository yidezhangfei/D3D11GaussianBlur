#ifndef _D3DUTIL_H_
#define _D3DUTIL_H_

#include <d3dcompiler.h>
#include <D3D11.h>
#include <d3dx11.h>

namespace d3d
{
  HRESULT InitD3D11(
    HINSTANCE hInstance,
    HWND hWnd,
    int width,
    int height,
    bool windowed);

  void EnterMsgLoop(
    void (*ptr_display)(float timeDelta));

  LRESULT CALLBACK WndProc(
    HWND hWnd,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam);

  void Render(float delteTime);

  void CleanupDevice();

  HRESULT CompileShaderFromFile(wchar_t* szFileName, LPCSTR szEntryPoint,
    LPCSTR szShaderModel, ID3DBlob** ppBlobOut);

  HRESULT CompileShaderFromMemory(LPCSTR shaderSrcData, LPCSTR szEntryPoint,
      LPCSTR szShaderModel, ID3DBlob** ppBlobOut);

  template<class T> void Release(T t)
  {
    if (t)
    {
      t->Release();
      t = NULL;
    }
  }

  template<class T> void Delete(T t)
  {
    if (t) 
    {
      delete t;
      t = NULL;
    }
  }
}  // namespace d3d

#endif // _D3DUTIL_H_