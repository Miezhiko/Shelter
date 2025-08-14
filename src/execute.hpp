#pragma once

#include "stdafx.hpp"

#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <sstream>
#include <array>
#include <expected>
#include <format>

using ShellResult = std::expected< std::string
                                 , std::string >;

[[nodiscard]] std::string
exec(const char* cmd) {
  #ifdef _WIN32
  std::array<char, 128> buffer;
  std::stringstream result;
  std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(cmd, "r"), _pclose);
  if (!pipe) {
    throw std::runtime_error("popen() failed!");
  }
  while ( fgets(buffer.data()
        , static_cast<int>(buffer.size()), pipe.get()) != nullptr ) {
    result << buffer.data();
  }
  return result.str();
  #else
  std::array<char, 128> buffer;
  std::stringstream result;
  std::unique_ptr<FILE, int (*)(FILE*)> pipe(popen(cmd, "r"), pclose);
  if (!pipe) {
    throw std::runtime_error("popen() failed!");
  }
  while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
    result << buffer.data();
  }
  return result.str();
  #endif
}

[[nodiscard]] constexpr std::string
trim_newlines(std::string str) noexcept {
  std::erase(str, '\n');
  return str;
}

[[nodiscard]] ShellResult
safe_exec(std::string_view command, bool trim = true) noexcept {
  try {
    return trim ? trim_newlines( exec(command.data()) )
                : exec(command.data());
  } catch (const std::exception& e) {
    return std::unexpected(std::format("Command '{}' failed: {}", command, e.what()));
  }
}
