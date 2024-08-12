#pragma once

static constexpr bool renderable =
#ifdef RENDERABLE
  true
#else
  false
#endif
  ;

#ifdef RENDERABLE

#include <core/renderables.hpp>

#endif
