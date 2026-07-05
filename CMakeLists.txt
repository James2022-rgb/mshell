cmake_minimum_required(VERSION 3.22)

#
# mshell -- "app shell" library: window + mnexus + ImGui + per-frame
# lifecycle. The shell owns nothing application-specific; consumers
# pass a `mshell::IFrame` that decides what to draw each frame.
#
# Layered on top of: mbase, mnexus, masset, mslang-proxy. Statically
# embeds Dear ImGui (incl. its SDL2 / Android backends) and
# ImGuiFileDialog. SDL2 is pulled in by the parent build (so apps
# can pick the version they want); mshell only assumes the
# `SDL2::SDL2-static` target exists on desktop platforms.
#

set(TARGET_NAME "mshell")
set(SRC_DIR "src")

project(${TARGET_NAME}
  VERSION 0.1.0.0
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)

#
# Required parent-provided targets. mshell asserts they exist (we
# don't pull them in via add_subdirectory; the parent stays in control
# of which versions to use).
#

if(NOT TARGET mbase)
  message(FATAL_ERROR "mshell: required target 'mbase' not defined. Add it before mshell.")
endif()
if(NOT TARGET mnexus)
  message(FATAL_ERROR "mshell: required target 'mnexus' not defined. Add it before mshell.")
endif()
if(NOT TARGET masset)
  message(FATAL_ERROR "mshell: required target 'masset' not defined. Add it before mshell.")
endif()
if(NOT TARGET mslang-proxy)
  message(FATAL_ERROR "mshell: required target 'mslang-proxy' not defined. Add it before mshell.")
endif()

#
# Dear ImGui -- compiled into mshell (which consumers then get via
# `target_link_libraries(... mshell ...)`).
#
# Source resolution, in order:
#   1. MSHELL_IMGUI_DIR, when the parent project sets it (points at a
#      vendored Dear ImGui checkout, docking branch).
#   2. The wentos-era sibling path `../wentos/thirdparty/imgui`, when
#      it exists (mshell consumed as a submodule of that repo).
#   3. FetchContent of the pinned docking tag.
#

if(NOT TARGET dear_imgui)
  if(NOT DEFINED MSHELL_IMGUI_DIR)
    set(_mshell_imgui_sibling "${CMAKE_CURRENT_SOURCE_DIR}/../wentos/thirdparty/imgui")
    if(EXISTS "${_mshell_imgui_sibling}/imgui.cpp")
      set(MSHELL_IMGUI_DIR "${_mshell_imgui_sibling}")
    endif()
  endif()
  if(NOT DEFINED MSHELL_IMGUI_DIR)
    include(FetchContent)
    FetchContent_Declare(dear_imgui_src
      GIT_REPOSITORY https://github.com/ocornut/imgui.git
      GIT_TAG        v1.91.7-docking
    )
    FetchContent_GetProperties(dear_imgui_src)
    if(NOT dear_imgui_src_POPULATED)
      FetchContent_Populate(dear_imgui_src)
    endif()
    set(MSHELL_IMGUI_DIR "${dear_imgui_src_SOURCE_DIR}")
  endif()
  set(IMGUI_DIR "${MSHELL_IMGUI_DIR}")
  set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imconfig.h
    ${IMGUI_DIR}/imgui.h
    ${IMGUI_DIR}/imgui_internal.h
    ${IMGUI_DIR}/imstb_rectpack.h
    ${IMGUI_DIR}/imstb_textedit.h
    ${IMGUI_DIR}/imstb_truetype.h
  )
  if(NOT ANDROID AND NOT EMSCRIPTEN)
    list(APPEND IMGUI_SOURCES
      ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
      ${IMGUI_DIR}/backends/imgui_impl_sdl2.h
    )
  endif()
  if(ANDROID)
    list(APPEND IMGUI_SOURCES
      ${IMGUI_DIR}/backends/imgui_impl_android.cpp
      ${IMGUI_DIR}/backends/imgui_impl_android.h
    )
  endif()

  add_library(dear_imgui STATIC ${IMGUI_SOURCES})
  set_target_properties(dear_imgui PROPERTIES FOLDER "external")
  target_include_directories(dear_imgui PUBLIC ${IMGUI_DIR})
  if(NOT ANDROID AND NOT EMSCRIPTEN)
    target_link_libraries(dear_imgui PRIVATE SDL2::SDL2-static)
  endif()
endif()

#
# ImGuiFileDialog (aiekick/ImGuiFileDialog) -- a native-looking, ImGui-
# rendered file dialog. Pulled in here so any app that links mshell
# gets a working File-Open dialog without duplicating the FetchContent
# block.
#

if(NOT TARGET ImGuiFileDialog)
  include(FetchContent)
  FetchContent_Declare(imgui_file_dialog
    GIT_REPOSITORY https://github.com/aiekick/ImGuiFileDialog.git
    GIT_TAG        v0.6.7
  )
  FetchContent_GetProperties(imgui_file_dialog)
  if(NOT imgui_file_dialog_POPULATED)
    FetchContent_Populate(imgui_file_dialog)
  endif()

  add_library(ImGuiFileDialog STATIC
    ${imgui_file_dialog_SOURCE_DIR}/ImGuiFileDialog.cpp
    ${imgui_file_dialog_SOURCE_DIR}/ImGuiFileDialog.h
    ${imgui_file_dialog_SOURCE_DIR}/ImGuiFileDialogConfig.h
  )
  set_target_properties(ImGuiFileDialog PROPERTIES FOLDER "external")
  target_include_directories(ImGuiFileDialog PUBLIC ${imgui_file_dialog_SOURCE_DIR})
  target_link_libraries(ImGuiFileDialog PUBLIC dear_imgui)
endif()

#
# mshell library proper
#

add_library(${TARGET_NAME} STATIC)
target_compile_features(${TARGET_NAME} PRIVATE cxx_std_23)

set(SOURCES_PUBLIC_ROOT_DIR  "${SRC_DIR}/${TARGET_NAME}/public")
set(SOURCES_PRIVATE_ROOT_DIR "${SRC_DIR}/${TARGET_NAME}/private")

set(SOURCES_MSHELL_PUBLIC
  ${SOURCES_PUBLIC_ROOT_DIR}/mshell.h
  ${SOURCES_PUBLIC_ROOT_DIR}/imgui_renderer.h
)
source_group("Public" FILES ${SOURCES_MSHELL_PUBLIC})

set(SOURCES_MSHELL_PRIVATE
  ${SOURCES_PRIVATE_ROOT_DIR}/mshell.cpp
  ${SOURCES_PRIVATE_ROOT_DIR}/imgui_renderer.cpp
  ${SOURCES_PRIVATE_ROOT_DIR}/slang_cache_asset.h
  ${SOURCES_PRIVATE_ROOT_DIR}/slang_cache_asset.cpp
  ${SOURCES_PRIVATE_ROOT_DIR}/pso_debug_imgui.h
  ${SOURCES_PRIVATE_ROOT_DIR}/pso_debug_imgui.cpp
)
source_group("Private" FILES ${SOURCES_MSHELL_PRIVATE})

target_sources(${TARGET_NAME} PRIVATE
  ${SOURCES_MSHELL_PUBLIC}
  ${SOURCES_MSHELL_PRIVATE}
)

target_include_directories(${TARGET_NAME} PUBLIC
  ${SRC_DIR}
)
target_include_directories(${TARGET_NAME} PRIVATE
  ${SOURCES_PRIVATE_ROOT_DIR}
)

target_link_libraries(${TARGET_NAME} PUBLIC
  mbase
  mnexus
  masset
  mslang-proxy
  dear_imgui
  ImGuiFileDialog
)
if(NOT EMSCRIPTEN AND NOT ANDROID)
  target_link_libraries(${TARGET_NAME} PUBLIC SDL2::SDL2-static)
endif()
