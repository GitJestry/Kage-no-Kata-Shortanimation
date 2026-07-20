cmake_minimum_required(VERSION 3.26..4.0)

project(kage_engine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
    Debug Release RelWithDebInfo MinSizeRel)
endif()

include(FetchContent)
include(CTest)
include(cmake/Sources.cmake)
include(cmake/PatchGltemplate.cmake)

file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}" KAGE_SOURCE_ROOT)

option(KAGE_WARNINGS_AS_ERRORS "Treat project warnings as build failures" OFF)
option(KAGE_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(KAGE_ENABLE_OPENGL_TESTS "Enable tests that create an OpenGL 4.1 context" OFF)

if(KAGE_ENABLE_SANITIZERS AND MSVC)
  message(FATAL_ERROR "KAGE_ENABLE_SANITIZERS requires Clang or GCC")
endif()
function(kage_apply_project_options TARGET_NAME)
  if(MSVC)
    target_compile_options(${TARGET_NAME} PRIVATE
      /W4 /permissive- /utf-8 /Zc:__cplusplus
      $<$<BOOL:${KAGE_WARNINGS_AS_ERRORS}>:/WX>)
  else()
    target_compile_options(${TARGET_NAME} PRIVATE -Wall -Wextra -Wpedantic
      $<$<BOOL:${KAGE_WARNINGS_AS_ERRORS}>:-Werror>)
  endif()
  if(KAGE_ENABLE_SANITIZERS)
    target_compile_options(${TARGET_NAME} PRIVATE
      -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(${TARGET_NAME} PRIVATE -fsanitize=address,undefined)
  endif()
endfunction()

if(MSVC)
  # getenv is used only to inspect PATH; C4702 originates in the pinned framework header.
  set_property(SOURCE src/film/film_exporter.cpp APPEND
    PROPERTY COMPILE_OPTIONS /wd4996 /wd4702)
  # The GLM vector indices in this translation unit are bounded to [0, 3].
  set_property(SOURCE src/render/gpu_mesh.cpp APPEND PROPERTY COMPILE_OPTIONS /wd4267)
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  # Timeline labels are bounded by the authored film duration.
  set_property(SOURCE src/editor/timeline_view_helpers.cpp APPEND
    PROPERTY COMPILE_OPTIONS -Wno-format-truncation)
endif()

set(FRAMEWORK_SOURCE_DIR "${CMAKE_BINARY_DIR}/_deps/framework-src")
set(FRAMEWORK_BINARY_DIR "${CMAKE_BINARY_DIR}/_deps/framework-build")
FetchContent_Populate(
  framework
  URL https://github.com/julcst/gltemplate/archive/v1.7b.tar.gz
  URL_HASH SHA256=78c8b4f4e192943be35c6cb672365c02832f35e63dd10ca1be6a28a9e01e8362
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  SOURCE_DIR "${FRAMEWORK_SOURCE_DIR}"
  BINARY_DIR "${FRAMEWORK_BINARY_DIR}"
)
kage_patch_gltemplate("${FRAMEWORK_SOURCE_DIR}")
add_subdirectory("${FRAMEWORK_SOURCE_DIR}" "${FRAMEWORK_BINARY_DIR}")

set(TINYGLTF_BUILD_LOADER_EXAMPLE OFF CACHE BOOL "" FORCE)
set(TINYGLTF_BUILD_GL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(TINYGLTF_BUILD_VALIDATOR_EXAMPLE OFF CACHE BOOL "" FORCE)
set(TINYGLTF_BUILD_BUILDER_EXAMPLE OFF CACHE BOOL "" FORCE)
set(TINYGLTF_INSTALL OFF CACHE BOOL "" FORCE)
set(TINYGLTF_HEADER_ONLY ON CACHE BOOL "" FORCE)
FetchContent_Declare(
  tinygltf
  URL https://github.com/syoyo/tinygltf/archive/refs/tags/v2.9.7.tar.gz
  URL_HASH SHA256=9d31cf7f22e81febaf1ad587d7722582c154f7d9125673ee46c0c594765e8f35
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(tinygltf)

FetchContent_Declare(
  meshoptimizer
  URL https://github.com/zeux/meshoptimizer/archive/refs/tags/v1.1.tar.gz
  URL_HASH SHA256=b787011f81b4b3069c2f9065b7c191efdd4189a49be32ba5282dd5579f05261a
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(MESHOPT_BUILD_DEMO OFF CACHE BOOL "" FORCE)
set(MESHOPT_BUILD_GLTFPACK OFF CACHE BOOL "" FORCE)
set(MESHOPT_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(meshoptimizer)

add_library(kage_engine_lib STATIC ${KAGE_ENGINE_SOURCES})
target_compile_features(kage_engine_lib PUBLIC cxx_std_23)
target_include_directories(kage_engine_lib PUBLIC src)
target_include_directories(kage_engine_lib SYSTEM PUBLIC
  "${FRAMEWORK_SOURCE_DIR}/src"
  "${FRAMEWORK_BINARY_DIR}/src")
target_link_libraries(kage_engine_lib PUBLIC framework tinygltf meshoptimizer)
target_compile_definitions(kage_engine_lib PRIVATE
  "KAGE_SOURCE_ROOT=\"${KAGE_SOURCE_ROOT}\"")
kage_apply_project_options(kage_engine_lib)

add_executable(${PROJECT_NAME} src/main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE kage_engine_lib)
kage_apply_project_options(${PROJECT_NAME})
target_compile_definitions(framework PRIVATE
  KAGE_GL_VALIDATION=$<IF:$<CONFIG:Debug>,1,0>)

function(kage_add_check TARGET_NAME)
  add_executable(${TARGET_NAME} ${ARGN})
  target_link_libraries(${TARGET_NAME} PRIVATE kage_engine_lib)
  kage_apply_project_options(${TARGET_NAME})
  target_compile_definitions(${TARGET_NAME} PRIVATE
    KAGE_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
  string(REGEX REPLACE "_check$" "" TEST_NAME "${TARGET_NAME}")
  add_test(NAME ${TEST_NAME} COMMAND ${TARGET_NAME})
  set_tests_properties(${TEST_NAME} PROPERTIES TIMEOUT 120)
endfunction()

if(BUILD_TESTING)
  kage_add_check(film_domain_check tests/film_domain_check.cpp)
  kage_add_check(film_persistence_check tests/film_persistence_check.cpp)
  kage_add_check(movie_workflow_check tests/movie_workflow_check.cpp)
  kage_add_check(math_camera_check tests/math_camera_check.cpp)
  kage_add_check(scene_editor_check tests/scene_editor_check.cpp)
  kage_add_check(asset_pipeline_check tests/asset_pipeline_check.cpp)
  kage_add_check(animation_lighting_check tests/animation_lighting_check.cpp)
  kage_add_check(runtime_paths_check tests/runtime_paths_check.cpp)

  if(KAGE_ENABLE_OPENGL_TESTS)
    kage_add_check(render_smoke_check tests/render_smoke_check.cpp)
    kage_add_check(application_smoke_check tests/application_smoke_check.cpp)
    set_tests_properties(render_smoke application_smoke PROPERTIES
      LABELS "opengl;integration" TIMEOUT 600)
  endif()
endif()

if(APPLE)
  # OpenGL is deprecated by macOS, but remains the required course API.
  target_compile_definitions(kage_engine_lib PUBLIC GL_SILENCE_DEPRECATION)
elseif(WIN32)
  # Keep Windows headers small and prevent their min/max macros from leaking.
  target_compile_definitions(kage_engine_lib PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX)
elseif(NOT WIN32)
  message(WARNING "The supported project platforms are macOS and Windows")
endif()

set(RUNTIME_ASSET_DIRECTORIES
  animations
  audio
  models
  textures
)

option(KAGE_COPY_RUNTIME_ASSETS
  "Copy runtime assets beside the executable after each build" OFF)
if(KAGE_COPY_RUNTIME_ASSETS)
  foreach(ASSET_DIRECTORY IN LISTS RUNTIME_ASSET_DIRECTORIES)
    add_custom_command(
      TARGET ${PROJECT_NAME}
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/${ASSET_DIRECTORY}"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets/${ASSET_DIRECTORY}"
      COMMENT "Copying runtime ${ASSET_DIRECTORY} assets"
      VERBATIM
    )
  endforeach()
endif()

install(TARGETS ${PROJECT_NAME}
  RUNTIME DESTINATION .
  BUNDLE DESTINATION .
)

foreach(ASSET_DIRECTORY IN LISTS RUNTIME_ASSET_DIRECTORIES)
  install(
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets/${ASSET_DIRECTORY}"
    DESTINATION assets
  )
endforeach()
