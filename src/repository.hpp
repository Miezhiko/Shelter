#pragma once

#include "options.hpp"
#include "execute.hpp"
#include "utils.hpp"

#include <filesystem>
#include <unordered_map>
#include <string_view>
#include <ranges>
#include <algorithm>
#include <sstream>

static constexpr std::string_view OPTIONS_FILE = ".shelter_options.yml";
static constexpr std::string_view CONFIG_FILE  = ".shelter.yml";

enum class [[nodiscard]] VCS { Git, Pijul, GitShell };
enum class [[nodiscard]] Action { Pull, Rebase, Unknown };

static constexpr std::array<std::pair<std::string_view, Action>, 2>
STRACTION_ARRAY = {{
  { "pull",    Action::Pull   },
  { "rebase",  Action::Rebase }
}};

static constexpr std::array<std::pair<std::string_view, std::string_view>, 3>
MIGMA_ARRAY = {{
  { ".migma.py",    "python" },
  { ".migma.sh",    "bash"   },
  { ".migma.pl",    "perl"   }
}};

[[nodiscard]] constexpr std::string_view
action_to_string(Action a) noexcept {
  for (const auto& [name, action] : STRACTION_ARRAY) {
    if (action == a) return name;
  }
  return "Unknown";
}

std::ostream& operator<<(std::ostream& os, const Action& a) {
  const auto str = action_to_string(a);
  if (str == "Unknown") {
    os << std::format("Unknown ({})", static_cast<std::underlying_type_t<Action>>(a));
  } else {
    os << str;
  }
  return os;
}

class [[nodiscard]] RepoArgs {
  std::string target_;
  std::string upstream_;
  std::string branch_;
  std::string remote_;
  Action action_;

  public:
  RepoArgs(std::string target, std::string action_str, std::string upstream, std::string branch, std::string remote = {})
    : target_(std::move(target))
    , upstream_(std::move(upstream))
    , branch_(std::move(branch))
    , remote_(std::move(remote)) {

    auto it = std::ranges::find_if(STRACTION_ARRAY,
      [&action_str](const auto& pair) { return pair.first == action_str; });
    if (it != STRACTION_ARRAY.end()) {
      action_ = it->second;
    } else {
      action_ = Action::Unknown;
    }
  }

  [[nodiscard]] const std::string& target()   const noexcept { return target_; }
  [[nodiscard]] const std::string& upstream() const noexcept { return upstream_; }
  [[nodiscard]] const std::string& branch()   const noexcept { return branch_; }
  [[nodiscard]] const std::string& remote()   const noexcept { return remote_; }
  [[nodiscard]] Action             action()   const noexcept { return action_; }
};

class [[nodiscard]] Repository {
  RepoArgs args_;
  std::string hash_;
  bool hash_updated_;

  [[nodiscard]] bool
  path_exists() const noexcept {
    std::error_code ec;
    return std::filesystem::exists(std::filesystem::path{args_.target()}, ec);
  }

  void
  migma(const std::shared_ptr<GlobalOptions>& opts) const noexcept {
    try {
      const std::filesystem::path target_path(args_.target());
      if (!path_exists()) return;

      for (const auto& [migma_file, interpreter] : MIGMA_ARRAY) {
        const std::filesystem::path migma_path = target_path / migma_file;
        std::error_code ec;
        if (std::filesystem::exists(migma_path, ec)) {
          // Execute from the repository directory
          const auto cmd = std::format("cd '{}' && {} {}",
            args_.target(), interpreter, migma_file);
          const auto output = safe_exec(cmd, false);
          if (output) {
            if (opts->is_verbose()) {
              std::cout << *output << '\n';
            }
          } else {
            std::cout << std::format("Failed to execute migma command '{}': {}\n",
              cmd, output.error());
          }
          break;
        }
      }
    } catch (const std::filesystem::filesystem_error& e) {
      std::cout << std::format("Migma filesystem error: {}\n", e.what());
    }
  }

  virtual void pull   (const std::shared_ptr<GlobalOptions>&) = 0;
  virtual void rebase (const std::shared_ptr<GlobalOptions>&) = 0;

  public:
  Repository(RepoArgs args, std::string hash)
    : args_(std::move(args)), hash_(std::move(hash)), hash_updated_(false) {}

  virtual ~Repository() = default;

  Repository(const Repository&)             = delete;
  Repository& operator=(const Repository&)  = delete;
  Repository(Repository&&)                  = default;
  Repository& operator=(Repository&&)       = default;

  [[nodiscard]] const std::string& target()   const noexcept { return args_.target();   }
  [[nodiscard]] const std::string& upstream() const noexcept { return args_.upstream(); }
  [[nodiscard]] const std::string& branch()   const noexcept { return args_.branch();   }
  [[nodiscard]] const std::string& remote()   const noexcept { return args_.remote();   }
  [[nodiscard]] const std::string& repo_hash() const noexcept { return hash_;            }
  [[nodiscard]] bool is_hash_updated() const noexcept { return hash_updated_;            }

  void set_hash(std::string new_hash) {
    if (hash_ != new_hash) {
      std::cout << std::format("new hash: {}\n", new_hash);
      hash_ = std::move(new_hash);
      hash_updated_ = true;
    }
  }

  void process(const std::shared_ptr<GlobalOptions>& opts) {
    if (!path_exists()) {
      std::cout << std::format("Target path '{}' does not exist\n", args_.target());
      return;
    }

    switch (args_.action()) {
      [[likely]] case Action::Pull:   pull(opts);   break;
      case Action::Rebase:             rebase(opts); break;
      [[unlikely]] case Action::Unknown:
        std::cout << "unknown task for " << args_.target() << std::endl;
        return;
    }

    if (is_hash_updated()) {
      migma(opts);
    }
  }

  [[nodiscard]] std::string details() const {
    return std::format("{} ({}) [{}]",
                      args_.target(),
                      args_.branch(),
                      action_to_string(args_.action()));
  }

  friend std::ostream& operator<<(std::ostream& os, const Repository& r) {
    os << r.args_.target();
    return os;
  }

  friend std::ostream& operator<<(std::ostream& os, const Repository* r) {
    if (r) { os << r->args_.target(); } else { os << "null"; }
    return os;
  }
};

template <VCS G>
class [[nodiscard]] Repo final : public Repository {
  void pull   (const std::shared_ptr<GlobalOptions>&) override;
  void rebase (const std::shared_ptr<GlobalOptions>&) override;

  public:
  Repo(RepoArgs args, std::string hash)
    : Repository(std::move(args), std::move(hash)) {}
};
