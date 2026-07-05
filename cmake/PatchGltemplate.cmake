function(kage_replace_in_file parFile parOldText parNewText)
  file(READ "${parFile}" file_content)
  string(FIND "${file_content}" "${parNewText}" new_text_position)
  if(NOT new_text_position EQUAL -1)
    return()
  endif()

  string(FIND "${file_content}" "${parOldText}" old_text_position)
  if(old_text_position EQUAL -1)
    message(FATAL_ERROR "Expected gltemplate text not found in ${parFile}")
  endif()

  string(REPLACE "${parOldText}" "${parNewText}" file_content
    "${file_content}"
  )
  file(WRITE "${parFile}" "${file_content}")
endfunction()

function(kage_patch_gltemplate parSourceDir)
  set(fetch_dependencies_file
    "${parSourceDir}/cmake/FetchDependencies.cmake"
  )
  set(framework_cmake_file "${parSourceDir}/src/framework/CMakeLists.txt")
  set(context_file "${parSourceDir}/src/framework/context.hpp.in")

  kage_replace_in_file("${fetch_dependencies_file}" [=[
    URL https://github.com/glfw/glfw/archive/refs/tags/3.4.tar.gz
    EXCLUDE_FROM_ALL
    FIND_PACKAGE_ARGS # First try to find the package in the system, if not found download it locally. For example use `brew install glfw` on macOS
]=] [=[
    URL https://github.com/glfw/glfw/archive/refs/tags/3.4.tar.gz
    URL_HASH SHA256=c038d34200234d071fae9345bc455e4a8f2f544ab60150765d7704e08f3dac01
    EXCLUDE_FROM_ALL
]=])

  kage_replace_in_file("${fetch_dependencies_file}" [=[
    URL https://github.com/g-truc/glm/archive/1.0.1.tar.gz
    EXCLUDE_FROM_ALL
    FIND_PACKAGE_ARGS # First try to find the package in the system, if not found download it locally. For example use `brew install glm` on macOS
]=] [=[
    URL https://github.com/g-truc/glm/archive/1.0.1.tar.gz
    URL_HASH SHA256=9f3174561fd26904b23f0db5e560971cbf9b3cbda0b280f04d5c379d03bf234c
    EXCLUDE_FROM_ALL
]=])

  kage_replace_in_file("${fetch_dependencies_file}" [=[
    URL https://github.com/ocornut/imgui/archive/v1.91.9b.tar.gz
    EXCLUDE_FROM_ALL
]=] [=[
    URL https://github.com/ocornut/imgui/archive/v1.91.9b.tar.gz
    URL_HASH SHA256=8e1bbc76c71d74fef2fb85db7e7ca8eba13d6a86623c54992b60162db554ffdb
    EXCLUDE_FROM_ALL
]=])

  kage_replace_in_file("${fetch_dependencies_file}" [=[
    URL https://github.com/tinyobjloader/tinyobjloader/archive/v2.0.0rc13.tar.gz
    EXCLUDE_FROM_ALL
]=] [=[
    URL https://github.com/tinyobjloader/tinyobjloader/archive/v2.0.0rc13.tar.gz
    URL_HASH SHA256=0feb92b838f8ce4aa6eb0ccc32dff30cb64a891e0ec3bde837fca49c78d44334
    EXCLUDE_FROM_ALL
]=])

  kage_replace_in_file("${fetch_dependencies_file}" [=[
file(DOWNLOAD https://raw.githubusercontent.com/nothings/stb/master/stb_image.h ${stb_SOURCE_DIR}/stb_image.h)
file(DOWNLOAD https://raw.githubusercontent.com/nothings/stb/master/stb_image_write.h  ${stb_SOURCE_DIR}/stb_image_write.h)
]=] [=[
file(DOWNLOAD
    https://raw.githubusercontent.com/nothings/stb/31c1ad37456438565541f4919958214b6e762fb4/stb_image.h
    ${stb_SOURCE_DIR}/stb_image.h
    EXPECTED_HASH SHA256=594c2fe35d49488b4382dbfaec8f98366defca819d916ac95becf3e75f4200b3
    TLS_VERIFY ON)
file(DOWNLOAD
    https://raw.githubusercontent.com/nothings/stb/31c1ad37456438565541f4919958214b6e762fb4/stb_image_write.h
    ${stb_SOURCE_DIR}/stb_image_write.h
    EXPECTED_HASH SHA256=cbd5f0ad7a9cf4468affb36354a1d2338034f2c12473cf1a8e32053cb6914a05
    TLS_VERIFY ON)
]=])

  kage_replace_in_file("${framework_cmake_file}" [=[
# Enables OpenGL 4.6 when not on macOS, else falls back to 4.1
if(NOT APPLE)
]=] [=[
# OpenGL 4.1 is the portable baseline shared by macOS and Windows.
option(GLTEMPLATE_ENABLE_MODERN_GL "Enable the OpenGL 4.6 code path" OFF)
if(GLTEMPLATE_ENABLE_MODERN_GL AND NOT APPLE)
]=])

  kage_replace_in_file("${context_file}" [=[
    const std::filesystem::path APP_DIR = HOME / "/Library/Application Support/${CMAKE_PROJECT_NAME}/"; // Writing into the .app bundle is not allowed as it is signed
]=] [=[
    const std::filesystem::path APP_DIR = HOME / "Library" / "Application Support" / "${CMAKE_PROJECT_NAME}"; // Writing into the .app bundle is not allowed as it is signed
]=])

  kage_replace_in_file("${context_file}" [=[
inline std::string getCWDWarning() {
    std::stringstream stream;
    const auto cwd = std::filesystem::current_path();
    if (std::filesystem::current_path() != SOURCE_DIR) {
        stream << "Warning: The current working directory " << cwd << " is not the source directory " << SOURCE_DIR
               << ". This can lead to relative paths not being resolved as expected, you can fix this by setting the working directory with `cd` before running your program." << std::endl;
    }
    return stream.str();
}
]=] [=[
inline std::string getCWDWarning() {
    #ifndef NDEBUG
        std::stringstream stream;
        const auto cwd = std::filesystem::current_path();
        if (cwd != SOURCE_DIR) {
            stream << "Warning: The current working directory " << cwd << " is not the source directory " << SOURCE_DIR
                   << ". This can lead to relative paths not being resolved as expected, you can fix this by setting the working directory with `cd` before running your program." << std::endl;
        }
        return stream.str();
    #else
        return {};
    #endif
}
]=])
endfunction()
