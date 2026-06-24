cmake_minimum_required(VERSION 3.26..4.0)

project(kage_no_kata VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SRC
  src/app/main_app.cpp
  src/main.cpp
)

include(FetchContent)
find_package(Git REQUIRED)
FetchContent_Declare(
  framework
  URL https://github.com/julcst/gltemplate/archive/v1.7b.tar.gz
  URL_HASH SHA256=78c8b4f4e192943be35c6cb672365c02832f35e63dd10ca1be6a28a9e01e8362
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  PATCH_COMMAND
    "${GIT_EXECUTABLE}" apply --whitespace=nowarn
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/patches/gltemplate-v1.7b.patch"
)
FetchContent_MakeAvailable(framework)

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

add_executable(${PROJECT_NAME} ${SRC})
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_include_directories(${PROJECT_NAME} PRIVATE src)
target_link_libraries(${PROJECT_NAME} PRIVATE framework tinygltf)

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
  audio
  models
  textures
)

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
