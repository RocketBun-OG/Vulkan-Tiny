
CFLAGS = -std=c++17 -g -o2
LDFLAGS = -lglfw3 -lgdi32 -lvulkan-1
RELEASEFLAGS = -DNDEBUG
VulkanAdventures: *.cpp ; g++ $(CFLAGS) -o bin\VulkanAdventures *.cpp $(LDFLAGS)

release: *.cpp ; g++ $(CFLAGS) $(RELEASEFLAGS) -o bin\VulkanAdventures *.cpp $(LDFLAGS) 
