#include "film/film_exporter.hpp"

#include <framework/gl/framebuffer.hpp>
#include <framework/gl/texture.hpp>

#include <glad/gl.h>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

namespace {

[[nodiscard]] std::string quoteArgument(const std::filesystem::path& parPath) {
#if defined(_WIN32)
  std::string value = parPath.string();
  std::string quoted = "\"";
  for (const char character : value) {
    quoted += character == '"' ? "\\\"" : std::string(1, character);
  }
  return quoted + "\"";
#else
  const std::string value = parPath.string();
  std::string quoted = "'";
  for (const char character : value) {
    quoted += character == '\'' ? "'\\''" : std::string(1, character);
  }
  return quoted + "'";
#endif
}

[[nodiscard]] bool executableFile(const std::filesystem::path& parPath) {
  std::error_code error;
  return !parPath.empty() && std::filesystem::is_regular_file(parPath, error);
}

}  // namespace

namespace kage::film {

struct FinalRenderJob::Impl final {
  FinalRenderState state = FinalRenderState::Idle;
  std::filesystem::path frame_directory;
  std::filesystem::path movie_path;
  std::filesystem::path ffmpeg;
  std::string error;
  int start_frame = 0;
  int end_frame = 0;
  int next_frame = 0;
  Framebuffer framebuffer;
  Texture<GL_TEXTURE_2D> color;
  Texture<GL_TEXTURE_2D> depth;

  void fail(std::string parError) {
    error = std::move(parError);
    state = FinalRenderState::Error;
    Framebuffer::bindDefault();
  }
};

FinalRenderJob::FinalRenderJob() : m_impl(std::make_unique<Impl>()) {}
FinalRenderJob::~FinalRenderJob() = default;

bool FinalRenderJob::start(const MovieTimeline& parTimeline,
                           std::filesystem::path parFrameDirectory,
                           std::filesystem::path parMoviePath,
                           std::filesystem::path parFfmpeg,
                           std::string& parError) {
  parError.clear();
  if (isActive()) {
    parError = "A final render is already running";
    return false;
  }
  const TimelineValidation validation = validateMovieTimeline(parTimeline, true);
  if (validation.hasErrors()) {
    const auto diagnostic = std::find_if(
        validation.diagnostics.begin(), validation.diagnostics.end(),
        [](const TimelineDiagnostic& item) {
          return item.severity == TimelineDiagnostic::Severity::Error;
        });
    parError = diagnostic != validation.diagnostics.end()
                   ? diagnostic->message
                   : "Movie is not valid for final render";
    return false;
  }
  if (!executableFile(parFfmpeg)) {
    parError = "ffmpeg was not found; install it or add it to PATH";
    return false;
  }
  std::error_code error;
  if (std::filesystem::exists(parFrameDirectory, error) &&
      !std::filesystem::is_empty(parFrameDirectory, error)) {
    parError = "Final frame directory is not empty";
    return false;
  }
  std::filesystem::create_directories(parFrameDirectory, error);
  std::filesystem::create_directories(parMoviePath.parent_path(), error);
  if (error) {
    parError = "Could not create final render output directories";
    return false;
  }

  m_impl = std::make_unique<Impl>();
  m_impl->frame_directory = std::move(parFrameDirectory);
  m_impl->movie_path = std::move(parMoviePath);
  m_impl->ffmpeg = std::move(parFfmpeg);
  m_impl->end_frame = parTimeline.durationFrames();
  m_impl->next_frame = 0;
  m_impl->color.allocate2D(GL_RGBA8, 3840, 2160, 1);
  m_impl->depth.allocate2D(GL_DEPTH_COMPONENT24, 3840, 2160, 1);
  m_impl->framebuffer.attach(GL_COLOR_ATTACHMENT0, m_impl->color);
  m_impl->framebuffer.attach(GL_DEPTH_ATTACHMENT, m_impl->depth);
  m_impl->framebuffer.checkStatus();
  m_impl->state = FinalRenderState::Rendering;
  return true;
}

void FinalRenderJob::advance(const MovieTimeline& parTimeline,
                             const FilmRenderFunction& parRender) {
  if (m_impl->state != FinalRenderState::Rendering) {
    return;
  }
  const int frame = m_impl->next_frame;
  m_impl->framebuffer.bind();
  glViewport(0, 0, 3840, 2160);
  parRender(frame, 3840, 2160, m_impl->framebuffer.handle);
  std::ostringstream filename;
  filename << "frame_" << std::setw(6) << std::setfill('0') << frame
           << ".png";
  if (!m_impl->framebuffer.writeToFile(m_impl->frame_directory /
                                       filename.str())) {
    m_impl->fail("Could not write final frame " + std::to_string(frame));
    return;
  }
  Framebuffer::bindDefault();
  ++m_impl->next_frame;
  if (m_impl->next_frame < m_impl->end_frame) {
    return;
  }

  std::ofstream manifest(m_impl->frame_directory / "manifest.json");
  manifest << "{\n"
           << "  \"name\": \"" << parTimeline.name << "\",\n"
           << "  \"fps\": 30,\n"
           << "  \"width\": 3840,\n"
           << "  \"height\": 2160,\n"
           << "  \"start_frame\": 0,\n"
           << "  \"end_frame\": " << m_impl->end_frame << "\n"
           << "}\n";
  if (!manifest) {
    m_impl->fail("Could not write final render manifest");
    return;
  }
  const std::filesystem::path pattern =
      m_impl->frame_directory / "frame_%06d.png";
  const std::string command =
      quoteArgument(m_impl->ffmpeg) +
      " -y -framerate 30 -start_number 0 -i " + quoteArgument(pattern) +
      " -c:v libx264 -preset slow -crf 14 -pix_fmt yuv420p"
      " -movflags +faststart " + quoteArgument(m_impl->movie_path);
  if (std::system(command.c_str()) != 0) {
    m_impl->fail("ffmpeg could not encode the final MPEG4 movie");
    return;
  }
  m_impl->state = FinalRenderState::Complete;
}

void FinalRenderJob::cancel() {
  if (isActive()) {
    m_impl->state = FinalRenderState::Cancelled;
    Framebuffer::bindDefault();
  }
}

FinalRenderState FinalRenderJob::getState() const { return m_impl->state; }

float FinalRenderJob::getProgress() const {
  return m_impl->end_frame > m_impl->start_frame
             ? static_cast<float>(m_impl->next_frame - m_impl->start_frame) /
                   static_cast<float>(m_impl->end_frame - m_impl->start_frame)
             : 0.0f;
}

const std::string& FinalRenderJob::getError() const { return m_impl->error; }

bool FinalRenderJob::isActive() const {
  return m_impl->state == FinalRenderState::Rendering;
}

std::filesystem::path findFfmpegExecutable() {
  const char* path_value = std::getenv("PATH");
  if (path_value != nullptr) {
#if defined(_WIN32)
    constexpr char SEPARATOR = ';';
    constexpr const char* NAME = "ffmpeg.exe";
#else
    constexpr char SEPARATOR = ':';
    constexpr const char* NAME = "ffmpeg";
#endif
    std::stringstream paths(path_value);
    std::string directory;
    while (std::getline(paths, directory, SEPARATOR)) {
      const std::filesystem::path candidate =
          std::filesystem::path(directory) / NAME;
      if (executableFile(candidate)) {
        return candidate;
      }
    }
  }
#if defined(__APPLE__)
  for (const std::filesystem::path& candidate : {
           std::filesystem::path("/opt/homebrew/bin/ffmpeg"),
           std::filesystem::path("/usr/local/bin/ffmpeg")}) {
    if (executableFile(candidate)) {
      return candidate;
    }
  }
#endif
  return {};
}

}  // namespace kage::film
