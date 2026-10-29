/**
 ******************************************************************************
 * @file    Scene.hpp
 * @brief   Draws one PunchPixel frame from the Game state with the Painter.
 ******************************************************************************
 */

#ifndef PP_SCENE_HPP
#define PP_SCENE_HPP

#include <cstdint>

#include "Game.hpp"
#include "Painter.hpp"

namespace Pp
{

/// @p clockMs is wall time, for blinking that must go on while the game is paused.
void drawScene(Painter& p, const Game& g, uint32_t clockMs);

} // namespace Pp

#endif
