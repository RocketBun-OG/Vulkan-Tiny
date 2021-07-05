#include "vk_engine.h"
#include <cstdlib>
#include <iostream>

int main() {
  VulkanEngine engine;
  engine.init();

  engine.run();

  engine.cleanup();
}
