#pragma once
#include "../MiniEngine/Core/GameCore.h"

class D3DTest : public GameCore::IGameApp
{
public:
  D3DTest();
  ~D3DTest() override;

  void Startup() override;
  void Cleanup() override;
  void Update(float deltaT) override;
  void RenderScene() override;
  void RenderUI(GraphicsContext& Context) override;
};

