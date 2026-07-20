#include <cstdlib>
#include <exception>
#include <iostream>
#include <ostream>
#include <string>

#include "utility.h"

#if (defined(_WIN32) && defined(SUPPORT_DX_BUILD))
#include "dxApp.h"
#endif
#include "vulkanApp.h"
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

auto main(int argc, char *const argv[]) -> int
{
  std::string path = Utility::getExecutableDirectory(argv[0]);

  VulkanApp::App vkApp;
#if (defined(_WIN32) && defined(SUPPORT_DX_BUILD))
  DXApp::App dxApp;
  try
  {
    if (argc > 1 && std::string{argv[1]} == "-dx")
    {
      std::cout << "Running DX12 App" << std::endl;
      dxApp.run(path);
    }
    else
    {
      std::cout << "Running Vulkan App" << std::endl;
      vkApp.run(path);
    }
  }
#else
  try
  {
    vkApp.run(path);
  }
#endif
  catch (const std::exception &e)
  {
    std::cerr << e.what() << std::endl;
    std::cin.get(); // Pause before exit
    return EXIT_FAILURE;
  }

  std::cout << "Hello World!" << std::endl;

  return EXIT_SUCCESS;
}
