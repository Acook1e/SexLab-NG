#pragma once

#include "Define/Scene.h"

namespace Define
{
class AnimPack
{
public:
  AnimPack(const std::string& name, const std::string& author, const std::string& animPackTag,
           const std::vector<Scene>& scenes)
      : name(name), author(author), animPackTag(animPackTag), scenes(scenes)
  {}
  AnimPack(std::string&& name, std::string&& author, std::string&& animPackTag, std::vector<Scene>&& scenes)
      : name(std::move(name)), author(std::move(author)), animPackTag(std::move(animPackTag)), scenes(std::move(scenes))
  {}
  ~AnimPack() = default;

  [[nodiscard]] const std::string& GetName() const { return name; }
  [[nodiscard]] const std::string& GetAuthor() const { return author; }
  [[nodiscard]] const std::string& GetAnimPackTag() const { return animPackTag; }
  [[nodiscard]] const std::vector<Scene>& GetScenes() const { return scenes; }

  [[nodiscard]] const std::string verbose() const
  {
    std::string res;
    res += "AnimPack: " + name + "\n";
    res += "Author: " + author + "\n";
    res += "AnimPackTag: " + animPackTag + "\n";
    res += std::format("Scenes: {}\n", scenes.size());
    for (const auto& scene : scenes) {
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