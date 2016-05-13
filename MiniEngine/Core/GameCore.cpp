//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
// Developed by Minigraph
//
// Author:  James Stanard 
//

#include "pch.h"
#include "GameCore.h"
#include "GraphicsCore.h"
#include "SystemTime.h"
#include "GameInput.h"
#include "BufferManager.h"
#include "CommandContext.h"
#include "PostEffects.h"

namespace Graphics
{
  extern ColorBuffer g_GenMipsBuffer;
}

namespace GameCore
{
  using namespace Graphics;
  const bool TestGenerateMips = false;

  void InitializeApplication(IGameApp& game)
  {
    Initialize();
    SystemTime::Initialize();
    GameInput::Initialize();
    EngineTuning::Initialize();

    game.Startup();
  }

  void TerminateApplication(IGameApp& game)
  {
    game.Cleanup();

    GameInput::Shutdown();
  }

  bool UpdateApplication(IGameApp& game)
  {
    EngineProfiling::Update();

    float DeltaTime = GetFrameTime();

    GameInput::Update(DeltaTime);
    EngineTuning::Update(DeltaTime);

    game.Update(DeltaTime);
    game.RenderScene();

    PostEffects::Render();

    if (TestGenerateMips)
    {
      GraphicsContext& MipsContext = GraphicsContext::Begin();

      // Exclude from timings this copy necessary to setup the test
      MipsContext.CopySubresource(g_GenMipsBuffer, 0, g_SceneColorBuffer, 0);

      EngineProfiling::BeginBlock(L"GenerateMipMaps()", &MipsContext);
      g_GenMipsBuffer.GenerateMipMaps(MipsContext);
      EngineProfiling::EndBlock(&MipsContext);

      MipsContext.Finish();
    }

    // Xbox One has an separate image plane that we use for UI.  It will composite with the
    // main image plane, so we need to clear it each from (assuming it's being dynamically
    // updated.)

    GraphicsContext& UiContext = GraphicsContext::Begin(L"Render UI");
    UiContext.ClearColor(g_OverlayBuffer);
    UiContext.SetRenderTarget(g_OverlayBuffer);
    UiContext.SetViewportAndScissor(0, 0, g_OverlayBuffer.GetWidth(), g_OverlayBuffer.GetHeight());
    game.RenderUI(UiContext);

    EngineTuning::Display(UiContext, 10.0f, 40.0f, 1900.0f, 1040.0f);

    UiContext.Finish();

    Present();

    if (IsFirstPressed(GameInput::kKey_escape))
    {
      return false; // shutdown
    }
    return true;
  }


  void InitWindow(const wchar_t* className);
  LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
  LRESULT CALLBACK WndProc2(HWND, UINT, WPARAM, LPARAM);

  //===========================================================================
  HWND g_hWnd = nullptr;
  WNDPROC MessageHandler = WndProc;

  static bool s_gameRunning = false;
  static bool s_fullscreen = false;

  //===========================================================================
  void RunApplication(IGameApp& app, const wchar_t* className)
  {
    // prevents the message handler from doiung things until the 
    // app is initialised
    s_gameRunning = true;

    HINSTANCE hInst = GetModuleHandle(nullptr);

    // Register class
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = MessageHandler;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInst;
    wcex.hIcon = LoadIcon(hInst, IDI_APPLICATION);
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wcex.lpszMenuName = nullptr;
    wcex.lpszClassName = className;
    wcex.hIconSm = LoadIcon(hInst, IDI_APPLICATION);
    ASSERT(0 != RegisterClassEx(&wcex), "Unable to register a window");

    // Create window
    RECT rc = { 0, 0, static_cast<LONG>(g_DisplayWidth), static_cast<LONG>(g_DisplayHeight) };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

    if (s_fullscreen == false)
    {
      g_hWnd = CreateWindow(className, className, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInst, nullptr);
    }
    else
    {
      g_hWnd = CreateWindow(className, className, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInst, nullptr);
    }

    ASSERT(g_hWnd != nullptr);

    InitializeApplication(app);

    if (s_fullscreen == false)
    {
      ShowWindow(g_hWnd, SW_SHOWDEFAULT);
    }
    else
    {
      ShowWindow(g_hWnd, SW_SHOWMAXIMIZED);
    }

    s_gameRunning = true;

    do
    {
      MSG msg = {};
      while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
      {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }
      
      if (msg.message == WM_QUIT)
        break;

    } while (UpdateApplication(app));	// Returns false to quit loop

    s_gameRunning = false;

    Terminate();
    TerminateApplication(app);
    Shutdown();
  }

  //===========================================================================
  LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
  {
    switch (message)
    {
    case WM_PAINT:
    {
      PAINTSTRUCT ps;
      HDC hdc = BeginPaint(hWnd, &ps);
      EndPaint(hWnd, &ps);
      break;
    }

    case WM_SIZE:
      Resize((UINT)(UINT64)lParam & 0xFFFF, (UINT)(UINT64)lParam >> 16);
      break;

    case WM_DESTROY:
      PostQuitMessage(0);
      break;

    default:
      return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
  }

  //===========================================================================
  LRESULT CALLBACK WndProc2(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
  {
    PAINTSTRUCT ps;
    HDC hdc;

    static bool s_in_sizemove = false;
    static bool s_in_suspend = false;
    static bool s_minimized = false;

    switch (message)
    {
    case WM_PAINT:
      hdc = BeginPaint(hWnd, &ps);
      EndPaint(hWnd, &ps);
      break;

    case WM_SIZE:
      if (wParam == SIZE_MINIMIZED)
      {
        if (!s_minimized && s_gameRunning)
        {
          s_minimized = true;
          if (!s_in_suspend)
            OnSuspending();
          s_in_suspend = true;
        }
      }
      else if (s_minimized)
      {
        s_minimized = false;
        if (s_in_suspend && s_gameRunning)
          OnResuming();
        s_in_suspend = false;
      }
      else if (!s_in_sizemove && s_gameRunning)
      {
        Resize(LOWORD(lParam), HIWORD(lParam));
      }
      break;

    case WM_ENTERSIZEMOVE:
      s_in_sizemove = true;
      break;

    case WM_EXITSIZEMOVE:
      s_in_sizemove = false;
      if (s_gameRunning)
      {
        RECT rc;
        GetClientRect(hWnd, &rc);

        Resize(rc.right - rc.left, rc.bottom - rc.top);
      }
      break;

    case WM_GETMINMAXINFO:
    {
      auto info = reinterpret_cast<MINMAXINFO*>(lParam);
      info->ptMinTrackSize.x = 320;
      info->ptMinTrackSize.y = 200;
    }
    break;

    case WM_ACTIVATEAPP:
      if (s_gameRunning)
      {
        if (wParam)
        {
          OnActivated();
        }
        else
        {
          OnDeactivated();
        }
      }
      break;

    case WM_POWERBROADCAST:
      switch (wParam)
      {
      case PBT_APMQUERYSUSPEND:
        if (!s_in_suspend && s_gameRunning)
          OnSuspending();
        s_in_suspend = true;
        return true;

      case PBT_APMRESUMESUSPEND:
        if (!s_minimized)
        {
          if (s_in_suspend && s_gameRunning)
            OnResuming();
          s_in_suspend = false;
        }
        return true;
      }
      break;

    case WM_DESTROY:
      PostQuitMessage(0);
      break;

    case WM_SYSKEYDOWN:
      if (wParam == VK_RETURN && (lParam & 0x60000000) == 0x20000000)
      {
        // Implements the classic ALT+ENTER fullscreen toggle
        if (s_fullscreen && s_gameRunning)
        {
          SetWindowLongPtr(hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);
          SetWindowLongPtr(hWnd, GWL_EXSTYLE, 0);

          int width = 800;
          int height = 600;

          GetDefaultSize(width, height);

          ShowWindow(hWnd, SW_SHOWNORMAL);

          SetWindowPos(hWnd, HWND_TOP, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
        }
        else
        {
          SetWindowLongPtr(hWnd, GWL_STYLE, 0);
          SetWindowLongPtr(hWnd, GWL_EXSTYLE, WS_EX_TOPMOST);

          SetWindowPos(hWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

          ShowWindow(hWnd, SW_SHOWMAXIMIZED);
        }

        s_fullscreen = !s_fullscreen;
      }
      break;
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
  }
}

