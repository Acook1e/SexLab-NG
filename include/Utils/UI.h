#pragma once

#include "Instance/SceneInstance.h"

class UI
{
public:
  static UI& GetSingleton()
  {
    static UI singleton;
    return singleton;
  }

  UI();
  ~UI();

  bool IsVisible() const { return isActive; }

  // 场景生命周期回调
  void Show(Instance::SceneInstance* scene);
  void Update(Instance::SceneInstance* scene);
  void Hide(Instance::SceneInstance* scene);

  // Alt 键控制焦点（鼠标可拖拽 UI）
  void SetFocus(bool focused);
  bool HasFocus() const;

  // 从 JS 侧接收的拖拽位置保存
  void OnPositionChanged(float x, float y);
  void OnSceneSelected(std::uintptr_t scenePtr);

private:
  bool isActive  = false;
  bool isFocused = false;

  // 持久化的 HUD 位置（用户拖拽后保存）
  float posX = -1.f;  // -1 表示使用默认位置
  float posY = -1.f;

  Instance::SceneInstance* currentScene = nullptr;

  void CreateView();
  void DestroyView();

  // 构建 JSON 并发送给 JS
  void SendInitData(Instance::SceneInstance* scene);
  void SendUpdateData(Instance::SceneInstance* scene);
};