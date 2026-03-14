#pragma once

namespace Define
{
// Define enjoyment levels for interactions
// from -100 to +100
enum class EnjoyDegree : int8_t
{
  ExtremlyPain  = -100,
  Pain          = -75,
  Hate          = -50,
  Uncomfortable = -25,
  NoFeeling     = 0,
  Good          = 25,
  Pleasure      = 50,
  Sensitive     = 75,
  Orgasm        = 100
};
}  // namespace Define