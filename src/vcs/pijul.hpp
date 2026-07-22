#pragma once

#include "repository.hpp"

template <> void
Repo <VCS::Pijul> :: pull (
  const std::shared_ptr<GlobalOptions>& opts
) {
  const auto pull_result = safe_exec(std::format("cd '{}' && pijul pull", target()), false);
  if (!pull_result) {
    sync_cout() << std::format("Pull failed: {}\n", pull_result.error());
    return;
  }
  if (opts->is_verbose()) {
    sync_cout() << *pull_result << std::endl;
  }
}

template <> void
Repo <VCS::Pijul> :: rebase (
  const std::shared_ptr<GlobalOptions>&
) {
  sync_cout() << "NOT IMPLEMENTED!" << std::endl;
}
