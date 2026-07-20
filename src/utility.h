#pragma once

#include <cstdint>
#include <string>
#include <vector>

constexpr uint32_t WIDTH  = 800;
constexpr uint32_t HEIGHT = 600;

class Utility
{
public:
  static std::string       getExecutableDirectory(char const *argv0);
  static std::vector<char> readFile(const std::string &filename);
};
