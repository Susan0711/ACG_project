cmake_minimum_required(VERSION 3.16)
project(forest_creek_cabin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(MSVC)
  add_compile_options(/W4 /permissive-)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(Threads REQUIRED)
add_library(geometry src/core/geometry.cpp)
target_include_directories(geometry PUBLIC src)
target_compile_features(geometry PUBLIC cxx_std_17)

add_library(acceleration src/acceleration/bvh.cpp)
target_link_libraries(acceleration PUBLIC geometry)

add_library(textures src/textures/texture.cpp)
target_include_directories(textures PUBLIC src)

add_library(materials src/materials/material.cpp)
target_link_libraries(materials PUBLIC textures)

add_library(tracer
  src/io/assets.cpp
  src/lighting/environment.cpp src/lighting/lights.cpp
  src/world.cpp src/render_scene.cpp
  src/render/path_integrator.cpp src/render/direct_lighting.cpp src/render/medium.cpp
  src/render/camera.cpp src/render/film.cpp src/render/render.cpp)
target_link_libraries(tracer PUBLIC acceleration materials Threads::Threads)

add_executable(normal_preview src/test/normal_main.cpp)
target_link_libraries(normal_preview PRIVATE geometry)
add_library(scene_input src/scene_init.cpp src/materials/material_reader.cpp)
target_include_directories(scene_input SYSTEM PRIVATE third_party)
target_link_libraries(scene_input PUBLIC tracer)
add_executable(path_tracer src/main.cpp)
target_link_libraries(path_tracer PRIVATE scene_input)

include(CTest)
if(BUILD_TESTING)
  add_executable(bvh_compare EXCLUDE_FROM_ALL tests/bvh_compare.cpp)
  target_link_libraries(bvh_compare PRIVATE scene_input)
  add_executable(mis_compare EXCLUDE_FROM_ALL tests/mis_compare.cpp)
  target_link_libraries(mis_compare PRIVATE scene_input)

  add_library(scene_library
    tests/scenes/cornell.cpp tests/scenes/forest.cpp tests/scenes/forest_v03.cpp tests/scenes/forest_v05.cpp
    tests/scenes/catalog.cpp tests/scenes/texture_scene.cpp tests/scenes/effect_scenes.cpp tests/scenes/sampling_scene.cpp)
  target_link_libraries(scene_library PUBLIC tracer)

  function(add_renderer_test name library)
    add_executable(${name}_tests tests/${name}_tests.cpp)
    target_link_libraries(${name}_tests PRIVATE ${library})
    add_test(NAME ${name} COMMAND ${name}_tests)
    set_tests_properties(${name} PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
  endfunction()
  add_renderer_test(geometry geometry)
  add_renderer_test(tracer scene_library)
  add_renderer_test(principled scene_input)
  target_include_directories(principled_tests SYSTEM PRIVATE third_party)
  add_renderer_test(pipeline scene_input)
  add_renderer_test(mipmap scene_input)
  add_renderer_test(effects tracer)
  add_renderer_test(render scene_library)
  add_renderer_test(environment scene_library)
  add_renderer_test(catalog scene_library)
  add_renderer_test(basic tracer)
  add_test(NAME scene_file COMMAND "${CMAKE_COMMAND}"
    "-DTRACER=$<TARGET_FILE:path_tracer>" "-DPROJECT_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/scene_file.cmake")
endif()
