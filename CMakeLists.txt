cmake_minimum_required(VERSION 3.22)

#
# mshell -- "app shell" library: window + mnexus + ImGui + per-frame
# lifecycle. The shell owns nothing application-specific; consumers
# pass a `mshell::IFrame` that decides what to draw each frame.
#
# Used by:
#   - wentos-app2 (PlaygroundHarnessFrame impl, hosts the playground
#                  manager + every built-in playground)
#   - gopro-viewer-lib (GoProViewerFrame impl, dedicated GoPro viewer)
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
# Dear ImGui -- compiled into mshell (which both apps then consume via
# `target_link_libraries(... mshell ...)`). The repo's vendored ImGui
# lives at wentos/thirdparty/imgui; the path is relative to this lib.
#

if(NOT TARGET dear_imgui)
  set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../wentos/thirdparty/imgui")
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
)
if(NOT EMSCRIPTEN AND NOT ANDROID)
  target_link_libraries(${TARGET_NAME} PUBLIC SDL2::SDL2-static)
endif()
