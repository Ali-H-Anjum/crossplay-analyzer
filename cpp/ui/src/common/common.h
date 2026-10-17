#pragma once

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_sdlrenderer2.h>
#include <SDL.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

extern const unsigned char kWordListData[];
extern const std::size_t kWordListSize;
extern const unsigned char kFontMonoData[];
extern const std::size_t kFontMonoSize;
extern const unsigned char kFontFranklinData[];
extern const std::size_t kFontFranklinSize;

extern ImFont* g_fontMono;
extern ImFont* g_fontFranklin;