cmake_minimum_required(VERSION 3.10)
project(shm_imgui_demo LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ImGui and GLAD directories (generated / cloned). Adjust if layout changes.
set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/../external/imgui")
set(GLAD_DIR "${CMAKE_SOURCE_DIR}/vendor/glad")

if(NOT EXISTS "${IMGUI_DIR}/imgui.cpp")
  message(FATAL_ERROR "Cannot find imgui.cpp at ${IMGUI_DIR}. Clone imgui into external/imgui")
endif()
if(NOT EXISTS "${GLAD_DIR}/src/gl.c")
  message(FATAL_ERROR "Cannot find gl.c at ${GLAD_DIR}/src. Generate GLAD (glad2) or adjust path")
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(GLFW REQUIRED glfw3)

include_directories(
  ${IMGUI_DIR}
  ${IMGUI_DIR}/backends
  ${GLAD_DIR}/include
  ${CMAKE_SOURCE_DIR}/../
)

set(IMGUI_SRC
  ${IMGUI_DIR}/imgui.cpp
  ${IMGUI_DIR}/imgui_demo.cpp
  ${IMGUI_DIR}/imgui_draw.cpp
  ${IMGUI_DIR}/imgui_tables.cpp
  ${IMGUI_DIR}/imgui_widgets.cpp
  ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
  ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

set(GLAD_SRC ${GLAD_DIR}/src/gl.c)

add_executable(shm_imgui
  main.cpp
  ${IMGUI_SRC}
  ${GLAD_SRC}
)

target_include_directories(shm_imgui PRIVATE ${GLFW_INCLUDE_DIRS})
target_link_libraries(shm_imgui PRIVATE ${GLFW_LIBRARIES} dl X11 GL pthread)

target_compile_definitions(shm_imgui PRIVATE "IMGUI_IMPL_OPENGL_LOADER_GLAD2")
