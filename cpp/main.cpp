#include "common/common.h"
#include "common/dock.h"
#include "common/theme.h"
#include "panels/bot_search_panel.h"
#include "panels/history_panel.h"
#include "panels/move_gen_panel.h"
#include "panels/popups.h"
#include "screens/sim_ui.h"
#include "screens/exploration.h"
#include "screens/pvb.h"
#include "screens/sim_csv.h"
#include "screens/sim_game.h"
#include "screens/sim_mover.h"
#include "screens/sim_stats.h"
#include "panels/save_load.h"
#include "screens/title.h"
#include "state/board_rules.h"
#include "state/state.h"
#include "widgets/drag_drop.h"
#include "widgets/screen_shell.h"
#include "widgets/top_strip.h"
#include "widgets/tray_widget.h"

// Global font pointers (defined here, declared extern in common.h)
ImFont* g_fontMono = nullptr;
ImFont* g_fontFranklin = nullptr;

// ================================================================ main
static std::string MaterializeEmbeddedWordList() {
  namespace fs = std::filesystem;
  std::error_code ec;
  fs::path dir = fs::temp_directory_path(ec);
  if (ec)
    dir = fs::current_path(ec);
  fs::path out = dir / "vision_wordlist.txt";
  if (fs::exists(out, ec) &&
      (std::size_t)fs::file_size(out, ec) == kWordListSize)
    return out.string();
  std::ofstream f(out, std::ios::binary | std::ios::trunc);
  if (!f)
    return {};
  f.write(reinterpret_cast<const char*>(kWordListData),
          (std::streamsize)kWordListSize);
  return f ? out.string() : std::string{};
}

int main(int /*argc*/, char** /*argv*/) {
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
    std::printf("SDL_Init: %s\n", SDL_GetError());
    return 1;
  }

  // Load the dictionary first so a missing file produces a clear message
  // instead of a crash.
  std::unique_ptr<Gaddag> gaddag;
  {
    const std::string path = MaterializeEmbeddedWordList();
    std::string failure;
    if (path.empty()) {
      failure =
          "Could not find wordList.txt.\nPass the path as the first argument, "
          "set "
          "VISION_WORDLIST, "
          "or place wordList.txt next to the executable.";
    } else {
      try {
        gaddag = std::make_unique<Gaddag>(path.c_str());
      } catch (const std::exception& ex) {
        failure = std::string("Failed to load word list:\n") + path + "\n" +
                  ex.what();
      }
    }
    if (!gaddag) {
      SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Vision", failure.c_str(),
                               nullptr);
      SDL_Quit();
      return 1;
    }
    g_gaddag = gaddag.get();
  }

  // Query the display's usable bounds so the window fits on smaller screens
  // (laptops) as well as large desktops.
  SDL_DisplayMode dm{};
  int displayW = 1440, displayH = 900;
  if (SDL_GetCurrentDisplayMode(0, &dm) == 0) {
    displayW = dm.w;
    displayH = dm.h;
  }

  // Target size we'd like if the screen is big enough.
  constexpr int kTargetW = 1920, kTargetH = 1080;
  // Absolute minimum below which the layout gets cramped; will be relaxed if
  // the display is smaller than this.
  constexpr int kMinAbsoluteW = 1500, kMinAbsoluteH = 840;
  // Leave headroom for the OS taskbar / window chrome.
  const int maxFitW = std::max(800, displayW - 20);
  const int maxFitH = std::max(600, displayH - 60);

  const int kDefaultW = std::min(kTargetW, maxFitW);
  const int kDefaultH = std::min(kTargetH, maxFitH);
  const int kMinW = std::min(kMinAbsoluteW, kDefaultW);
  const int kMinH = std::min(kMinAbsoluteH, kDefaultH);

  SDL_Window* window = SDL_CreateWindow(
      "Vision", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, kDefaultW,
      kDefaultH, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
  if (!window) {
    std::printf("SDL_CreateWindow: %s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }
  SDL_SetWindowMinimumSize(window, kMinW, kMinH);

  SDL_Renderer* renderer = SDL_CreateRenderer(
      window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
  if (!renderer) {
    std::printf("SDL_CreateRenderer: %s\n", SDL_GetError());
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  io.IniFilename = nullptr;  // dock layouts are rebuilt (and locked) in code;
                             // don't persist them
  ImGui::StyleColorsDark();

  {
    ImGuiIO& io2 = ImGui::GetIO();
    ImFontConfig cfg;
    cfg.OversampleH = 2;
    cfg.OversampleV = 2;
    cfg.PixelSnapH = true;
    cfg.FontDataOwnedByAtlas = false;
    g_fontMono = io2.Fonts->AddFontFromMemoryTTF(
        const_cast<unsigned char*>(kFontMonoData), (int)kFontMonoSize, 15.0f,
        &cfg);
    g_fontFranklin = io2.Fonts->AddFontFromMemoryTTF(
        const_cast<unsigned char*>(kFontFranklinData), (int)kFontFranklinSize,
        15.0f, &cfg);
    if (!g_fontMono)
      g_fontMono = io2.Fonts->AddFontDefault();
    if (!g_fontFranklin)
      g_fontFranklin = g_fontMono;
  }

  ImGuiStyle& st = ImGui::GetStyle();
  st.WindowRounding = 6.0f;
  st.FrameRounding = 6.0f;
  st.GrabRounding = 6.0f;
  st.WindowPadding = ImVec2(10, 10);
  st.FramePadding = ImVec2(10, 6);
  ImVec4* c = st.Colors;
  c[ImGuiCol_WindowBg] = ImVec4(0.15f, 0.15f, 0.18f, 1.00f);
  c[ImGuiCol_ChildBg] = ImVec4(0.13f, 0.13f, 0.16f, 1.00f);
  c[ImGuiCol_PopupBg] = ImVec4(0.15f, 0.15f, 0.18f, 0.98f);
  c[ImGuiCol_Border] = ImVec4(0.25f, 0.25f, 0.30f, 1.00f);
  c[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.20f, 0.24f, 1.00f);
  c[ImGuiCol_FrameBgHovered] = ImVec4(0.25f, 0.25f, 0.30f, 1.00f);
  c[ImGuiCol_FrameBgActive] = ImVec4(0.30f, 0.30f, 0.36f, 1.00f);
  c[ImGuiCol_TitleBg] = ImVec4(0.13f, 0.13f, 0.16f, 1.00f);
  c[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.15f, 0.18f, 1.00f);
  c[ImGuiCol_MenuBarBg] = ImVec4(0.13f, 0.13f, 0.16f, 1.00f);
  c[ImGuiCol_Header] = ImVec4(0.30f, 0.45f, 0.60f, 1.00f);
  c[ImGuiCol_HeaderHovered] = ImVec4(0.35f, 0.52f, 0.68f, 1.00f);
  c[ImGuiCol_HeaderActive] = ImVec4(0.28f, 0.42f, 0.58f, 1.00f);
  c[ImGuiCol_Button] = ImVec4(0.30f, 0.42f, 0.56f, 1.00f);
  c[ImGuiCol_ButtonHovered] = ImVec4(0.36f, 0.50f, 0.65f, 1.00f);
  c[ImGuiCol_ButtonActive] = ImVec4(0.26f, 0.38f, 0.50f, 1.00f);
  c[ImGuiCol_Text] = ImVec4(0.92f, 0.92f, 0.94f, 1.00f);
  c[ImGuiCol_TextDisabled] = ImVec4(0.55f, 0.55f, 0.60f, 1.00f);

  ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
  ImGui_ImplSDLRenderer2_Init(renderer);

  bool running = true;
  while (running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      ImGui_ImplSDL2_ProcessEvent(&e);
      if (e.type == SDL_QUIT)
        running = false;
    }
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    // Esc: cancel drag, then close popups, then leave the mode. Ignored while
    // typing.
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) && !io.WantTextInput) {
      if (g_ui.dragging) {
        EndDrag();
      } else if (g_ui.blankPopupOpen || g_ui.addTilePopupOpen ||
                 g_ui.savePopupOpen || g_ui.loadPopupOpen ||
                 g_ui.howToPlayOpen || g_ui.confirmBackOpen ||
                 g_pvbUi.firstPlayerDialog) {
        g_ui.blankPopupOpen = g_ui.addTilePopupOpen = g_ui.savePopupOpen =
            g_ui.loadPopupOpen = false;
        g_ui.howToPlayOpen = g_ui.confirmBackOpen = g_pvbUi.firstPlayerDialog =
            false;
      } else if (g_mode != AppMode::Title) {
        if (g_mode == AppMode::PlayerVsBot)
          RequestRematchOrTitle(ConfirmAction::Title);
        else
          LeaveToTitle();
      }
    }

    {
      ImGuiViewport* vp = ImGui::GetMainViewport();
      ImGui::SetNextWindowPos(vp->WorkPos);
      ImGui::SetNextWindowSize(vp->WorkSize);
      ImGui::SetNextWindowViewport(vp->ID);
      ImGuiWindowFlags flags =
          ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
          ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
          ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
          ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBackground;
      ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
      ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
      ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
      ImGui::Begin("##DockHost", nullptr, flags);
      ImGui::PopStyleVar(3);
      ImGui::DockSpace(DockIdFor(g_mode), ImVec2(0, 0),
                       ImGuiDockNodeFlags_None);
      ImGui::End();
    }

    switch (g_mode) {
      case AppMode::Title:
        DrawTitleScreen();
        break;
      case AppMode::Exploration:
        DrawExplorationMode();
        DrawAddTilePopup();
        break;
      case AppMode::PlayerVsBot:
        DrawPvbMode();
        break;
      case AppMode::BotVsBot:
        DrawSimMode();
        break;
    }

    DrawBlankPopup();
    DrawSavePopup();
    DrawLoadPopup();
    DrawHowToPlay();
    DrawConfirmBackPopup();

    // Board hover tooltip, drawn at the top level (outside any window or child)
    // so it renders correctly on the SDL2/SDLRenderer2 backend. See
    // DrawBoardHoverTooltip.
    if (g_mode == AppMode::Exploration)
      DrawBoardHoverTooltip(g_ex);

    // End-of-frame drag housekeeping: a drag that wasn't consumed by a drop
    // target (released off the board) is cancelled; otherwise draw the ghost on
    // top of everything.
    if (g_ui.dragging && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
      EndDrag();
    DrawDragGhost();

    // Viewport-size check at end of frame: if the size changed, invalidate the
    // built dock flags so the *next* frame rebuilds with the new WorkSize.
    {
      const ImVec2 sz = ImGui::GetMainViewport()->WorkSize;
      if (g_ui.lastViewportW != 0.0f &&
          (std::abs(sz.x - g_ui.lastViewportW) > 40.0f ||
           std::abs(sz.y - g_ui.lastViewportH) > 40.0f)) {
        g_ui.dockBuiltEx = false;
        g_ui.dockBuiltPvb = false;
        g_ui.dockBuiltSim = false;
      }
      g_ui.lastViewportW = sz.x;
      g_ui.lastViewportH = sz.y;
    }

    ImGui::Render();
    SDL_SetRenderDrawColor(renderer, 30, 30, 36, 255);
    SDL_RenderClear(renderer);
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
    SDL_RenderPresent(renderer);
  }

  g_bot.shutdown();
  g_gaddag = nullptr;
  ImGui_ImplSDLRenderer2_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
