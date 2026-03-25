#pragma once

namespace Instance
{
class Interact
{
public:
  enum class InteractType : std::uint64_t
  {
    None = 0,
    Anal,            //
    Anal_Insert,     //
    Boobjob_Breast,  //
    Boobjob_Penis,   //
    Blowjob_Mouth,   // Mouth to Penis, at Mouth Position
    Blowjob_Penis,   // Mouth to Penis, at Penis Position
    Buttjob_Butt,    //
    Buttjob_Penis,   //
    Footjob_Foot,    //
    Footjob_Penis,   //
    Handjob_Hand,    // Hand to Penis, at Hand Position
    Handjob_Penis,
    Kiss,            // Head to Head
    Vaginal,         //
    Vaginal_Insert,  //

    Total
  };

  struct InteractionState
  {
    std::bitset<sizeof(InteractType) * CHAR_BIT> interactions;
  };

  Interact();

  void Initialize(std::vector<RE::Actor*> actors);
  void Update();

private:
  std::vector<RE::Actor*> actors;
};
}  // namespace Instance