#pragma once
// Vision -- Gaddag move explorer / Player-vs-Greedy / Greedy-vs-Greedy
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"
#include <SDL.h>

#include <algorithm>
#include <array>
#include <bit>
#include <bitset>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <numeric>
#include <random>
#include <string>
#include <system_error>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

#include "Letter.h"
#include "Board.h"
#include "Move.h"
#include "Tray.h"
#include "BoardOps.h"
#include "CrossChecks.h"
#include "Gaddag.h"
#include "TileBag.h"
#include "MoveGen.h"
#include "MoveEval.h"

extern const unsigned char kWordListData[];
extern const std::size_t kWordListSize;
extern const unsigned char kFontMonoData[];
extern const std::size_t kFontMonoSize;
extern const unsigned char kFontFranklinData[];
extern const std::size_t kFontFranklinSize;

static ImFont* g_fontMono = nullptr;
static ImFont* g_fontFranklin = nullptr;

namespace fs = std::filesystem;