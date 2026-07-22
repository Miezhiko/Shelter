#pragma once

#include <iostream>
#include <syncstream>

// Use in place of std::cout wherever output may be produced concurrently
// (e.g. while repositories are processed in parallel), so that lines from
// different threads don't get interleaved mid-write.
[[nodiscard]] inline std::osyncstream
sync_cout() noexcept {
  return std::osyncstream(std::cout);
}
