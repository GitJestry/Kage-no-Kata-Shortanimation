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

add_executable(${PROJECT_NAME} src/main.cpp ${KAGE_ENGINE_SOURCES})
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_include_directories(${PROJECT_NAME} PRIVATE src)
target_link_libraries(${PROJECT_NAME} PRIVATE framework tinygltf meshoptimizer)
target_compile_definitions(framework PRIVATE
  KAGE_GL_VALIDATION=$<IF:$<CONFIG:Debug>,1,0>)

function(kage_add_check TARGET_NAME)
  add_executable(${TARGET_NAME} ${ARGN})
  target_compile_features(${TARGET_NAME} PRIVATE cxx_std_23)
  target_include_directories(${TARGET_NAME} PRIVATE src)
  target_link_libraries(${TARGET_NAME} PRIVATE glm)
endfunction()

if(BUILD_TESTING)
  kage_add_check(film_domain_check
    tests/film_domain_check.cpp
    src/film/movie_timeline.cpp
    src/film/timeline_edit_service.cpp
  )
  kage_add_check(film_persistence_check
    tests/film_persistence_check.cpp
    src/film/movie_timeline.cpp
    src/film/movie_timeline_serializer.cpp
    src/film/timeline_edit_service.cpp
    src/lighting/lighting_system.cpp
    src/scene/world.cpp
  )
  kage_add_check(movie_workflow_check
    tests/movie_workflow_check.cpp
    src/assets/asset_registry.cpp
    src/assets/gltf_asset_loader.cpp
    src/camera/camera.cpp
    src/editor/movie_editor_controller.cpp
    src/engine/film_camera_creation.cpp
    src/film/movie_timeline.cpp
    src/film/movie_timeline_world_validation.cpp
    src/film/timeline_edit_service.cpp
    src/scene/scene_manager.cpp
    src/scene/world.cpp
  )
  target_link_libraries(film_persistence_check PRIVATE framework tinygltf)
  target_link_libraries(movie_workflow_check PRIVATE tinygltf)
  add_test(NAME film_domain COMMAND film_domain_check)
  add_test(NAME film_persistence COMMAND film_persistence_check)
  add_test(NAME movie_workflow COMMAND movie_workflow_check)
endif()

if(MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus)
else()
  target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(APPLE)
  # OpenGL is deprecated by macOS, but remains the required course API.
  target_compile_definitions(${PROJECT_NAME} PRIVATE GL_SILENCE_DEPRECATION)
elseif(WIN32)
  # Keep Windows headers small and prevent their min/max macros from leaking.
  target_compile_definitions(${PROJECT_NAME} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
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
