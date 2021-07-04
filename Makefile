
CFLAGS = -std=c++17 -g 
LDFLAGS = -lglfw3 -lgdi32 -lvulkan-1
RELEASEFLAGS = -std=c++17 -o2 -DNDEBUG
VulkanAdventures: *.cpp ; g++ $(CFLAGS) -o bin\VulkanAdventures *.cpp $(LDFLAGS)

release: *.cpp ; g++ $(RELEASEFLAGS) -o bin\VulkanAdventures *.cpp $(LDFLAGS) 
