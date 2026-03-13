#pragma once

#include "Define/Scene.h"

namespace Define
{
class AnimPack
{
public:
  AnimPack(std::string a_name, std::string author, std::string hash) : name(std::move(a_name)), author(std::move(author)), hash(std::move(hash)) {}
  ~AnimPack() = default;

  std::string GetName() const { return name; }
  std::string GetAuthor() const { return author; }
  std::string GetHash() const { return hash; }

  std::vector<Scene>& GetScenes() { return scenes; }
  void AddScene(Scene a_scene) { scenes.push_back(std::move(a_scene)); }

private:
  std::string name;
  std::string author;
  std::string hash;

  std::vector<Scene> scenes;
};
}  // namespace Define