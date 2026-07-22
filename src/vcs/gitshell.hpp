#pragma once

#include <ranges>
#include <string_view>
#include <algorithm>
#include <optional>

#include "repository.hpp"

namespace {
  [[nodiscard]] ShellResult
  shell_get_local_hash(std::string_view repo_path) noexcept {
    return safe_exec(std::format("cd '{}' && git log -n 1 --pretty=format:%H", repo_path));
  }

  [[nodiscard]] ShellResult
  shell_get_remote_hash(std::string_view repo_path, std::string_view upstream) noexcept {
    const auto ls_remote_result = safe_exec(std::format("cd '{}' && git ls-remote {}", repo_path, upstream));
    if (!ls_remote_result) {
      return ls_remote_result;
    }

    const auto& ls_remote = *ls_remote_result;
    if (const auto tab_pos = ls_remote.find('\t'); tab_pos != std::string::npos) {
      return ls_remote.substr(0, tab_pos);
    }
    return ls_remote;
  }

  [[nodiscard]] ShellResult
  shell_clean(std::string_view repo_path, bool verbose = false) noexcept {
    const auto reset_result = safe_exec(std::format("cd '{}' && git reset --hard", repo_path));
    if (!reset_result) {
      return reset_result;
    }

    const auto clean_result = safe_exec(std::format("cd '{}' && git clean -fxd", repo_path));
    if (!clean_result) {
      return clean_result;
    }

    if (verbose) {
      sync_cout() << std::format("{}\n{}\n", *reset_result, *clean_result);
    }

    return std::format("{}\n{}", *reset_result, *clean_result);
  }

  [[nodiscard]] ShellResult
  checkout_branch(std::string_view repo_path, std::string_view branch, bool verbose = false) noexcept {
    const auto checkout_result = safe_exec(std::format("cd '{}' && git checkout {}", repo_path, branch));
    if (verbose && checkout_result) {
      sync_cout() << *checkout_result << '\n';
    }
    return checkout_result;
  }

  [[nodiscard]] ShellResult
  pull_upstream(std::string_view repo_path, std::string_view upstream, bool verbose = false) noexcept {
    const auto pull_result = safe_exec(std::format("cd '{}' && git pull {}", repo_path, upstream), false);
    if (verbose && pull_result) {
      sync_cout() << *pull_result << '\n';
    }
    return pull_result;
  }

  [[nodiscard]] ShellResult
  pull_rebase_upstream(std::string_view repo_path, std::string_view upstream, bool verbose = false) noexcept {
    const auto pull_result = safe_exec(std::format("cd '{}' && git pull --rebase {}", repo_path, upstream), false);
    if (verbose && pull_result) {
      sync_cout() << *pull_result << '\n';
    }
    return pull_result;
  }

  [[nodiscard]] ShellResult
  force_push_branch(std::string_view repo_path, std::string_view remote, std::string_view branch, bool verbose = false) noexcept {
    const auto push_result = safe_exec(std::format("cd '{}' && git push --force {} {}", repo_path, remote, branch), false);
    if (verbose && push_result) {
      sync_cout() << *push_result << '\n';
    }
    return push_result;
  }

  [[nodiscard]] ShellResult
  get_current_branch(std::string_view repo_path) noexcept {
    return safe_exec(std::format("cd '{}' && git rev-parse --abbrev-ref HEAD", repo_path));
  }
}

namespace gitshell {
  [[nodiscard]] std::optional<std::string>
  get_branch() noexcept {
    const auto branch_result = safe_exec("git rev-parse --abbrev-ref HEAD");
    return branch_result ? std::make_optional(*branch_result) : std::nullopt;
  }
}

template <> void
Repo <VCS::GitShell> :: pull (
  const std::shared_ptr<GlobalOptions>& opts
) {
  const auto& repo_path   = target();
  const auto& repo_branch = branch();

  const auto current_branch = get_current_branch(repo_path);
  if (!current_branch) {
    sync_cout() << "Failed to get current branch\n";
    return;
  }

  if (*current_branch != repo_branch) {
    if (!opts->do_force()) {
      sync_cout() << std::format( "Not on {}, but on {}, skipping update!\n"
                              , repo_branch, *current_branch );
      return;
    }

    const auto checkout_result = checkout_branch(repo_path, repo_branch, opts->is_verbose());
    if (!checkout_result) {
      sync_cout() << std::format("Checkout failed: {}\n", checkout_result.error());
      return;
    }
  }

  auto local_hash = repo_hash();
  if (local_hash.empty()) {
    const auto hash_result = shell_get_local_hash(repo_path);
    if (!hash_result) {
      sync_cout() << std::format("Failed to get local hash: {}\n", hash_result.error());
      return;
    }
    local_hash = *hash_result;
    set_hash(local_hash);
  }

  const auto remote_hash_result = shell_get_remote_hash(repo_path, upstream());
  if (!remote_hash_result) {
    sync_cout() << std::format("Failed to get remote hash: {}\n", remote_hash_result.error());
    return;
  }

  const auto& remote_hash = *remote_hash_result;
  if (local_hash == remote_hash) {
    sync_cout() << "repository " << *this << " is up to date\n";
    return;
  }

  if (opts->do_clean()) {
    const auto clean_result = shell_clean(repo_path, opts->is_verbose());
    if (!clean_result) {
      sync_cout() << std::format("Clean failed: {}\n", clean_result.error());
      return;
    }
  }

  const auto pull_result = pull_upstream(repo_path, upstream(), opts->is_verbose());
  if (!pull_result) {
    sync_cout() << std::format("Pull failed: {}\n", pull_result.error());
    return;
  }

  set_hash(remote_hash);
}

template <> void
Repo <VCS::GitShell> :: rebase (
  const std::shared_ptr<GlobalOptions>& opts
) {
  const auto& repo_path   = target();
  const auto& repo_branch = branch();

  const auto current_branch = get_current_branch(repo_path);
  if (!current_branch) {
    sync_cout() << "Failed to get current branch\n";
    return;
  }

  if (*current_branch != repo_branch) {
    const auto checkout_result = checkout_branch(repo_path, repo_branch, opts->is_verbose());
    if (!checkout_result) {
      sync_cout() << std::format("Checkout failed: {}\n", checkout_result.error());
      return;
    }
  }

  const auto& push_remote = remote().empty() ? "origin" : remote();

  const auto fetch_result = safe_exec(std::format("cd '{}' && git fetch {} {}", repo_path, push_remote, repo_branch));
  if (!fetch_result) {
    sync_cout() << std::format("Fetch from {} failed: {}\n", push_remote, fetch_result.error());
    return;
  }

  const auto reset_result = safe_exec(std::format("cd '{}' && git reset --hard {}/{}", repo_path, push_remote, repo_branch));
  if (!reset_result) {
    sync_cout() << std::format("Reset to {}/{} failed: {}\n", push_remote, repo_branch, reset_result.error());
    return;
  }
  if (opts->is_verbose()) {
    sync_cout() << *reset_result << '\n';
  }

  auto local_hash = repo_hash();
  if (local_hash.empty()) {
    const auto hash_result = shell_get_local_hash(repo_path);
    if (!hash_result) {
      sync_cout() << std::format("Failed to get local hash: {}\n", hash_result.error());
      return;
    }
    local_hash = *hash_result;
    set_hash(local_hash);
  }

  const auto remote_hash_result = shell_get_remote_hash(repo_path, upstream());
  if (!remote_hash_result) {
    sync_cout() << std::format("Failed to get remote hash: {}\n", remote_hash_result.error());
    return;
  }

  const auto& remote_hash = *remote_hash_result;
  if (local_hash == remote_hash) {
    sync_cout() << "repository " << *this << " is up to date\n";
    return;
  }

  if (opts->do_clean()) {
    const auto clean_result = shell_clean(repo_path, opts->is_verbose());
    if (!clean_result) {
      sync_cout() << std::format("Clean failed: {}\n", clean_result.error());
      return;
    }
  }

  const auto pull_result = pull_rebase_upstream(repo_path, upstream(), opts->is_verbose());
  if (!pull_result) {
    sync_cout() << std::format("Rebase pull failed: {}\n", pull_result.error());
    return;
  }

  const auto push_result = force_push_branch(repo_path, push_remote, repo_branch, opts->is_verbose());
  if (!push_result) {
    sync_cout() << std::format("Force push failed: {}\n", push_result.error());
    return;
  }

  set_hash(remote_hash);
}
