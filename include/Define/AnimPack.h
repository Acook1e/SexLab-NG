#pragma once

#include "Define/Scene.h"

namespace Define
{
class AnimPack
{
public:
  AnimPack(std::string a_name, std::string author, std::string animPackTag)
      : name(std::move(a_name)), author(std::move(author)), animPackTag(std::move(animPackTag))
  {}
  ~AnimPack() = default;

  std::string GetName() const { return name; }
  std::string GetAuthor() const { return author; }
  std::string GetAnimPackTag() const { return animPackTag; }

  std::vector<Scene>& GetScenes() { return scenes; }
  void SetScenes(std::vector<Scene> a_scenes) { scenes = std::move(a_scenes); }

  std::string verbose()
  {
    std::string res;
    res += "AnimPack: " + name + "\n";
    res += "Author: " + author + "\n";
    res += "AnimPackTag: " + animPackTag + "\n";
    res += std::format("Scenes: {}\n", scenes.size());
    for (auto& scene : scenes) {
      res += scene.verbose();
    }
    return res;
  }

private:
  std::string name;
  std::string author;
  std::string animPackTag;
  std::vector<Scene> scenes;
};
}  // namespace Define