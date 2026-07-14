#pragma once

// DEPRECATED: This file is kept for backwards compatibility only.
// The splash screen system has been rewritten to use the scene-based SplashScreenNode.
// See: lupine/runtime/SplashScreenNode.hpp
//
// The new system integrates with the normal scene update loop instead of running
// a separate blocking loop. Splash screens are now created as scene nodes that
// automatically remove themselves when complete.

#include "SplashScreenNode.hpp"
