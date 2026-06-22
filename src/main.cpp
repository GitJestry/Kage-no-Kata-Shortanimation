#include "app/main_app.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>

int main() {
  try {
    kage::app::MainApp app;
    app.run();
  } catch (const std::exception& parException) {
    std::cerr << "Application error: " << parException.what() << '\n';
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
