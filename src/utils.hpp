#pragma once

#include "stdafx.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace utils {

[[nodiscard]] std::optional<std::string>
get_home_dir() noexcept {
  #ifdef _WIN32
    const char* home = std::getenv("USERPROFILE");
    if (!home || home[0] == '\0') {
      return std::nullopt;
    }
    return std::string(home);
  #else
    const char* home = std::getenv("HOME");
    if (!home || home[0] == '\0') {
      return std::nullopt;
    }
    return std::string(home);
  #endif
}

[[nodiscard]] std::string
get_config_path(const std::string& filename) {
  const auto home = get_home_dir();
  if (!home) {
    throw std::runtime_error("Unable to determine home directory");
  }
  return std::filesystem::path(*home) / filename;
}

}
