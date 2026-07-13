#pragma once

#include "film/movie_timeline.hpp"

#include <filesystem>
#include <functional>
#include <memory>
#include <string>

namespace kage::film {

using FilmRenderFunction =
    std::function<void(int parFrame, int parWidth, int parHeight,
                       unsigned int parDestinationFramebuffer)>;

enum class FinalRenderState { Idle, Rendering, Complete, Error, Cancelled };

class FinalRenderJob final {
 public:
  FinalRenderJob();
  ~FinalRenderJob();
  FinalRenderJob(const FinalRenderJob&) = delete;
  FinalRenderJob& operator=(const FinalRenderJob&) = delete;

  [[nodiscard]] bool start(const MovieTimeline& parTimeline,
                           std::filesystem::path parFrameDirectory,
                           std::filesystem::path parMoviePath,
                           std::filesystem::path parFfmpeg,
                           std::string& parError);
  void advance(const MovieTimeline& parTimeline,
               const FilmRenderFunction& parRender);
  void cancel();

  [[nodiscard]] FinalRenderState getState() const;
  [[nodiscard]] float getProgress() const;
  [[nodiscard]] const std::string& getError() const;
  [[nodiscard]] bool isActive() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

[[nodiscard]] std::filesystem::path findFfmpegExecutable();

}  // namespace kage::film
