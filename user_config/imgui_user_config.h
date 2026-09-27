#pragma once

#include "helpers/assert.hpp"

#define IM_ASSERT(_EXPR) XENO_ASSERT(_EXPR)
// The injected Switch module has no desktop shell, C terminal, or POSIX wall-clock runtime.
#define IMGUI_DISABLE_DEFAULT_SHELL_FUNCTIONS
#define IMGUI_DISABLE_TIME_FUNCTIONS
// Avoid newlib stdout/__getreent imports during ImGui context cleanup.
#define IMGUI_DISABLE_TTY_FUNCTIONS
