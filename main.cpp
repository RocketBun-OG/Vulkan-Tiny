
#include "vulkan_app.hpp"
#include <cstdlib>
#include <iostream>

int main() {
  vulkan_app learning;
  try {
    learning.run();
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}