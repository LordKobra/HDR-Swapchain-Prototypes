#include "utility.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#ifdef _WIN32
#else
#include <limits.h>
#include <unistd.h> //readlink
#endif

std::string Utility::getExecutableDirectory(char const *argv0)
{
#ifdef _WIN32
  std::filesystem::path path = std::filesystem::absolute(argv0);
  return std::filesystem::path(path).parent_path().string();
#else
  char    path[FILENAME_MAX];
  ssize_t count = readlink("/proc/self/exe", path, FILENAME_MAX);
  return std::filesystem::path(std::string(path, (count > 0) ? count : 0)).parent_path().string();
#endif
}

std::vector<char> Utility::readFile(const std::string &filename)
{
  std::ifstream file(filename, std::ios::ate | std::ios::binary);
  if (!file.is_open())
  {
    std::cout << filename << std::endl;
    throw std::runtime_error("failed to open file!");
  }

  std::vector<char> buffer(file.tellg());

  file.seekg(0, std::ios::beg);
  file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
  file.close();

  return buffer;
}