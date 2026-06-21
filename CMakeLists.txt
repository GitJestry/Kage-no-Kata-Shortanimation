cmake_minimum_required(VERSION 3.26..4.0)

project(kage_no_kata VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SRC
  src/main.cpp
)

include(FetchContent)
FetchContent_Declare(
  framework
  URL https://github.com/julcst/gltemplate/archive/v1.7b.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(framework)

add_executable(${PROJECT_NAME} ${SRC})
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_include_directories(${PROJECT_NAME} PRIVATE src)
target_link_libraries(${PROJECT_NAME} PRIVATE framework)

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
