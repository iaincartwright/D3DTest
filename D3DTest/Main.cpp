#include "pch.h"
#include "D3DTest.h"

//===========================================================================
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
  UNREFERENCED_PARAMETER(hPrevInstance);
  UNREFERENCED_PARAMETER(lpCmdLine);

  Microsoft::WRL::Wrappers::RoInitializeWrapper initialize(RO_INIT_MULTITHREADED);
  if (FAILED(initialize))
    return 1;

  auto container = new D3DTest();

  GameCore::RunApplication(*container, L"D3D Test One");

  CoUninitialize();

  delete container;

  return 0;
}

//===========================================================================

