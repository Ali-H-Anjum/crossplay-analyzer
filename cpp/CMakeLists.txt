cmake_minimum_required(VERSION 3.20)
project(crossplay_ui CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (MINGW)
    add_link_options(-static-libgcc -static-libstdc++ -static)
endif()

add_compile_options(-Wall -Wextra)

find_package(SDL2 CONFIG REQUIRED)

set(IMGUI_DIR    ${CMAKE_CURRENT_SOURCE_DIR}/../imgui)
set(ENGINE_DIR   ${CMAKE_CURRENT_SOURCE_DIR}/engine)
set(UI_SRC_DIR   ${CMAKE_CURRENT_SOURCE_DIR}/ui/src)

# All engine subfolders are include roots, so headers stay flat-importable
# (e.g. #include "Board.h" works regardless of which engine/ folder it lives in).
set(ENGINE_INCLUDE_DIRS
    ${ENGINE_DIR}/types
    ${ENGINE_DIR}/data
    ${ENGINE_DIR}/gaddag
    ${ENGINE_DIR}/board
    ${ENGINE_DIR}/moves
    ${ENGINE_DIR}/scoring
    ${ENGINE_DIR}/state
    ${ENGINE_DIR}/search
)

# Same idea for the UI: every subfolder is an include root.
set(UI_INCLUDE_DIRS
    ${UI_SRC_DIR}
    ${UI_SRC_DIR}/common
    ${UI_SRC_DIR}/state
    ${UI_SRC_DIR}/widgets
    ${UI_SRC_DIR}/panels
    ${UI_SRC_DIR}/screens
)

add_executable(crossplay_ui
    main.cpp
    ui/tools/diagram_root.cpp
    ui/src/wordList_embedded.cpp
    ui/src/font_mono_embedded.cpp
    ui/src/font_franklin_embedded.cpp
    ${ENGINE_DIR}/gaddag/gaddag.cpp
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdlrenderer2.cpp
)

target_include_directories(crossplay_ui PRIVATE
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${ENGINE_INCLUDE_DIRS}
    ${UI_INCLUDE_DIRS}
)

target_link_libraries(crossplay_ui PRIVATE
    SDL2::SDL2main
    SDL2::SDL2
)

set(WORDLIST_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/../data/wordlists/wordList.txt")
if (EXISTS "${WORDLIST_SOURCE}")
    add_custom_command(TARGET crossplay_ui POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${WORDLIST_SOURCE}"
                "$<TARGET_FILE_DIR:crossplay_ui>/wordList.txt"
        COMMENT "Copying wordList.txt next to the exe")
endif()

add_executable(engine_tests
    tests/test_main.cpp
    tests/test_smoke.cpp
    tests/test_premium.cpp
    tests/test_onplay.cpp
    tests/test_score.cpp
    tests/test_apply.cpp
    tests/test_bag.cpp
    tests/test_position.cpp
    tests/test_eval.cpp
    tests/test_swap.cpp
    tests/test_swapgen.cpp
    tests/test_search.cpp
    tests/test_iterative.cpp
    tests/test_job.cpp
    tests/test_order.cpp
    tests/test_tt.cpp
    tests/test_prune.cpp
    tests/test_det.cpp
    tests/test_mcts.cpp
    ${ENGINE_DIR}/gaddag/gaddag.cpp
)

target_include_directories(engine_tests PRIVATE
    ${ENGINE_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
)

get_filename_component(DIAG_DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../data/runs" ABSOLUTE)
target_compile_definitions(crossplay_ui PRIVATE DIAG_DATA_DIR="${DIAG_DATA_DIR}")

target_compile_definitions(engine_tests PRIVATE WORDLIST_PATH="${WORDLIST_SOURCE}")
