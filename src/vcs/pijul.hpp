#pragma once

#include "repository.hpp"

template <> void Repo <VCS::Pijul> :: pull (
  const std::shared_ptr<GlobalOptions>& opts
) {
  const auto pull_cmd = "pijul pull";
  const auto output = exec(pull_cmd);
  if (opts->is_verbose()) {
    std::cout << output << std::endl;
  }
}

template <> void Repo <VCS::Pijul> :: rebase (
  const std::shared_ptr<GlobalOptions>&
) {
  std::cout << "NOT IMPLEMENTED!" << std::endl;
}
