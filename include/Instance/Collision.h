#pragma once

namespace Instance
{

class Collision
{
public:
  static Collision& GetSingleton()
  {
    static Collision instance;
    return instance;
  }

  void AddActor(RE::Actor* actor);
  void RemoveActor(RE::Actor* actor);
  bool HasActor(RE::Actor* actor);

  static void ConfigureControllerForScene(RE::bhkCharacterController* a_controller);

private:
  std::shared_mutex mtx;
  std::unordered_set<RE::Actor*> actors;
};

}  // namespace Instance