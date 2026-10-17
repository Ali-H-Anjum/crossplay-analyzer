#pragma once
#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

#include "letter.h"
#include "move_eval.h"
// Part of the main.cpp split. Not standalone: main.cpp includes these in order.

// ================================================================ constants
static constexpr int kRackSize = 7;
// Must match the bingo bonus used by MoveEval (NYT Crossplay: 40).
static constexpr int kBingoBonus = 40;
static constexpr float kCell = 30.0f;
static constexpr float kGap = 3.0f;
static constexpr float kCellRound = 6.0f;  // corner radius of board squares
static constexpr uint64_t kExplorationSeed = 12345;
static constexpr int kBotWaitFrames =
    3;  // lets "BOT'S TURN" render before the bot moves

// ================================================================ palette
static const ImU32 COL_ACCENT = IM_COL32(107, 165, 219, 255);
static const ImU32 COL_TEXT = IM_COL32(235, 235, 240, 255);

static const ImU32 COL_BOARD_EMPTY = IM_COL32(242, 238, 237, 255);
static const ImU32 COL_CENTER = IM_COL32(35, 35, 35, 255);
static const ImU32 COL_TILE = IM_COL32(43, 84, 146, 255);
static const ImU32 COL_RACK_SHELF = IM_COL32(206, 204, 208, 255);
static const ImU32 COL_TILE_TEXT = IM_COL32(255, 255, 255, 255);

// Premium squares (sampled from the reference screenshot).
static const ImU32 COL_2L = IM_COL32(240, 235, 205, 255);
static const ImU32 COL_2L_TEXT = IM_COL32(122, 92, 41, 255);
static const ImU32 COL_3L = IM_COL32(223, 231, 208, 255);
static const ImU32 COL_3L_TEXT = IM_COL32(88, 102, 55, 255);
static const ImU32 COL_2W = IM_COL32(217, 225, 238, 255);
static const ImU32 COL_2W_TEXT = IM_COL32(72, 94, 139, 255);
static const ImU32 COL_3W = IM_COL32(231, 221, 232, 255);
static const ImU32 COL_3W_TEXT = IM_COL32(156, 35, 171, 255);

static const ImU32 COL_HEAT_0 = IM_COL32(232, 92, 92, 200);
static const ImU32 COL_HEAT_LOW = IM_COL32(240, 179, 92, 200);
static const ImU32 COL_HEAT_MID = IM_COL32(244, 227, 138, 200);
static const ImU32 COL_HEAT_HI = IM_COL32(168, 216, 168, 200);

static const ImVec4 PANEL_BG = ImVec4(0.98f, 0.976f, 0.965f, 1.0f);
static const ImVec4 PANEL_TEXT = ImVec4(0.15f, 0.15f, 0.18f, 1.0f);
static const ImVec4 PANEL_TEXT_DIM = ImVec4(0.45f, 0.45f, 0.50f, 1.0f);
static const ImVec4 PANEL_SEP = ImVec4(0.85f, 0.85f, 0.85f, 1.0f);

static constexpr ImGuiWindowFlags PANEL_FLAGS =
    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
    ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

// ================================================================ small
// drawing helpers
static int LetterPoints(Letter l) {
  return POINTS_PER_TILE[toIndex(l)];
}

// The default ImGui font has no bold weight, so fake one with two extra offset
// passes.
static void AddBoldText(ImDrawList* dl, ImFont* font, float size, ImVec2 p,
                        ImU32 col, const char* s) {
  const float o = std::max(1.0f, size * 0.035f);
  dl->AddText(font, size, p, col, s);
  dl->AddText(font, size, ImVec2(p.x + o, p.y), col, s);
  dl->AddText(font, size, ImVec2(p.x, p.y + o), col, s);
}

// Heavier fake-bold used for tile letters and premium square labels so they
// match the reference tile style. 5 offset passes vs AddBoldText's 3.
static void AddHeavyText(ImDrawList* dl, ImFont* font, float size, ImVec2 p,
                         ImU32 col, const char* s) {
  const float o = std::max(1.0f, size * 0.06f);
  dl->AddText(font, size, p, col, s);
  dl->AddText(font, size, ImVec2(p.x + o, p.y), col, s);
  dl->AddText(font, size, ImVec2(p.x, p.y + o), col, s);
  dl->AddText(font, size, ImVec2(p.x + o * 0.5f, p.y + o * 0.5f), col, s);
  dl->AddText(font, size, ImVec2(p.x + o, p.y + o), col, s);
}

// Tile silhouette used everywhere: top-left and bottom-right are the most
// rounded corners.
static void DrawTileBody(ImDrawList* dl, ImVec2 p0, float size, ImU32 col) {
  const ImVec2 p1(p0.x + size, p0.y + size);
  const float rTL = size * 0.15f, rTR = size * 0.06f, rBR = size * 0.25f,
              rBL = size * 0.10f;
  dl->PathArcTo(ImVec2(p0.x + rTL, p0.y + rTL), rTL, IM_PI, IM_PI * 1.5f);
  dl->PathArcTo(ImVec2(p1.x - rTR, p0.y + rTR), rTR, IM_PI * 1.5f,
                IM_PI * 2.0f);
  dl->PathArcTo(ImVec2(p1.x - rBR, p1.y - rBR), rBR, 0.0f, IM_PI * 0.5f);
  dl->PathArcTo(ImVec2(p0.x + rBL, p1.y - rBL), rBL, IM_PI * 0.5f, IM_PI);
  dl->PathFillConvex(col);
}

struct GlyphMetrics {
  ImFont* font = nullptr;
  float cell = -1.0f;
  ImVec2 letter[128];
  ImVec2 pts[16];
};
static GlyphMetrics& GlyphMetricsFor(ImFont* font, float cell) {
  static GlyphMetrics cache[4];
  static int next = 0;
  for (auto& m : cache)
    if (m.font == font && m.cell == cell)
      return m;
  GlyphMetrics& m = cache[next];
  next = (next + 1) & 3;
  m.font = font;
  m.cell = cell;
  for (auto& v : m.letter)
    v = ImVec2(-1.0f, -1.0f);
  for (auto& v : m.pts)
    v = ImVec2(-1.0f, -1.0f);
  return m;
}

// Letter bottom-left, point value as a superscript top-right. Everything scales
// with `cell`. The letter itself is drawn with a heavier stroke than
// AddBoldText's default so it matches the reference tile style; the point value
// keeps the standard weight.
static void DrawTileGlyph(ImDrawList* dl, ImVec2 p0, float cell, char ch,
                          int pts, bool showPts) {
  ImFont* font = g_fontFranklin ? g_fontFranklin : ImGui::GetFont();
  GlyphMetrics& gm = GlyphMetricsFor(font, cell);
  const float letterSize = cell * 0.60f;
  const float ptSize = cell * 0.30f;
  const ImU32 ink = showPts ? COL_TILE_TEXT : IM_COL32(168, 190, 240, 255);

  if (ch != ' ') {
    const char buf[2] = {ch, 0};
    const unsigned u = (unsigned char)ch;
    ImVec2 ts;
    if (u < 128) {
      ImVec2& v = gm.letter[u];
      if (v.x < 0.0f)
        v = font->CalcTextSizeA(letterSize, 10000.0f, 0.0f, buf);
      ts = v;
    } else {
      ts = font->CalcTextSizeA(letterSize, 10000.0f, 0.0f, buf);
    }
    const ImVec2 lp(p0.x + cell * 0.17f, p0.y + cell - ts.y - cell * 0.08f);
    AddHeavyText(dl, font, letterSize, lp, ink, buf);
  }

  const bool drawPts = showPts && pts > 0;
  const int n = drawPts ? pts : 0;
  char pb[8];
  std::snprintf(pb, sizeof pb, "%d", n);
  ImVec2 ps;
  if (n < 16) {
    ImVec2& v = gm.pts[n];
    if (v.x < 0.0f)
      v = font->CalcTextSizeA(ptSize, 10000.0f, 0.0f, pb);
    ps = v;
  } else {
    ps = font->CalcTextSizeA(ptSize, 10000.0f, 0.0f, pb);
  }
  AddBoldText(dl, font, ptSize,
              ImVec2(p0.x + cell - ps.x - cell * 0.10f, p0.y + cell * 0.06f),
              drawPts ? COL_TILE_TEXT : ink, pb);
}

// Five-point star (concave, so drawn as a triangle fan) for the center square.
static void DrawStar(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
  ImVec2 pt[10];
  for (int i = 0; i < 10; ++i) {
    const float a = -IM_PI * 0.5f + i * (IM_PI / 5.0f);
    const float rr = (i % 2 == 0) ? r : r * 0.42f;
    pt[i] = ImVec2(c.x + std::cos(a) * rr, c.y + std::sin(a) * rr);
  }
  for (int i = 0; i < 10; ++i)
    dl->AddTriangleFilled(c, pt[i], pt[(i + 1) % 10], col);
}

static ImU32 PremiumFillNyt(Mult m) {
  switch (m) {
    case MULT_DL:
      return COL_2L;
    case MULT_TL:
      return COL_3L;
    case MULT_DW:
      return COL_2W;
    case MULT_TW:
      return COL_3W;
    default:
      return 0;
  }
}
static ImU32 PremiumTextNyt(Mult m) {
  switch (m) {
    case MULT_DL:
      return COL_2L_TEXT;
    case MULT_TL:
      return COL_3L_TEXT;
    case MULT_DW:
      return COL_2W_TEXT;
    case MULT_TW:
      return COL_3W_TEXT;
    default:
      return COL_TEXT;
  }
}
static const char* PremiumLabelNyt(Mult m) {
  switch (m) {
    case MULT_DL:
      return "2L";
    case MULT_TL:
      return "3L";
    case MULT_DW:
      return "2W";
    case MULT_TW:
      return "3W";
    default:
      return "";
  }
}
static ImU32 HeatFill(int n) {
  if (n == 0)
    return COL_HEAT_0;
  if (n <= 5)
    return COL_HEAT_LOW;
  if (n <= 15)
    return COL_HEAT_MID;
  if (n <= 25)
    return COL_HEAT_HI;
  return COL_BOARD_EMPTY;  // 26 = no constraint
}

static void DrawSwatch(ImU32 color, const char* label) {
  ImVec2 p = ImGui::GetCursorScreenPos();
  float h = ImGui::GetTextLineHeight();
  ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + h, p.y + h), color);
  ImGui::GetWindowDrawList()->AddRect(p, ImVec2(p.x + h, p.y + h),
                                      IM_COL32(120, 120, 130, 255));
  ImGui::Dummy(ImVec2(h, h));
  ImGui::SameLine();
  ImGui::TextUnformatted(label);
}

// Popups are centered when they appear (so the user can still drag them aside);
// only fixed windows such as the title card pass ImGuiCond_Always.
static void CenterNextWindow(ImGuiCond cond = ImGuiCond_Appearing) {
  ImGuiViewport* vp = ImGui::GetMainViewport();
  ImVec2 center(vp->WorkPos.x + vp->WorkSize.x * 0.5f,
                vp->WorkPos.y + vp->WorkSize.y * 0.5f);
  ImGui::SetNextWindowPos(center, cond, ImVec2(0.5f, 0.5f));
}

// Floating windows must never dock into the locked layout.
static constexpr ImGuiWindowFlags POPUP_FLAGS =
    ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking;
