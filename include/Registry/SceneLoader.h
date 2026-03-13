#pragma once

namespace Registry
{
class SceneLoader
{
public:
  virtual ~SceneLoader() = default;

  virtual void LoadSceneData() = 0;
};

class SLSB_Loader : public SceneLoader
{
public:
  void LoadSceneData() override;
};

class SLAL_Loader : public SceneLoader
{
public:
  void LoadSceneData() override;

private:
  void LoadJSONFile(const std::ifstream& file);
};

}  // namespace Registry