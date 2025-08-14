#pragma once

#include "options.hpp"
#include "execute.hpp"

#include <filesystem>
#include <unordered_map>
#include <string_view>
#include <ranges>
#include <algorithm>
#include <sstream>

enum class [[nodiscard]] VCS { Git, Pijul, GitShell };
enum class [[nodiscard]] Action { Pull, Rebase, Unknown };

static constexpr std::array<std::pair<std::string_view, Action>, 2> 
STRACTION_ARRAY = {{
  { "pull",    Action::Pull   },
  { "rebase",  Action::Rebase }
}};

static const std::unordered_map<std::string, Action> 
STRACTION = {
  { "pull",    Action::Pull   },
  { "rebase",  Action::Rebase }
};

static constexpr std::array<std::pair<std::string_view, std::string_view>, 3>
MIGMA_ARRAY = {{
  { ".migma.py",    "python" },
  { ".migma.sh",    "bash"   },
  { ".migma.pl",    "perl"   }
}};

static const std::unordered_map<std::string, std::string> 
MIGMA = {
  { ".migma.py",    "python" },
  { ".migma.sh",    "bash"   },
  { ".migma.pl",    "perl"   }
};

[[nodiscard]] constexpr std::string_view 
action_to_string(Action a) noexcept {
  const auto it = std::ranges::find_if(STRACTION_ARRAY, 
    [a](const auto& pair) { return pair.second == a; });
  return it != STRACTION_ARRAY.end() ? it->first : "Unknown";
}

std::ostream& operator
<< (std::ostream& os, const Action& a) {
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
  Action action_;
  
  public:
  RepoArgs(std::string target, std::string action_str, std::string upstream, std::string branch)
    : target_(std::move(target))
    , upstream_(std::move(upstream))
    , branch_(std::move(branch)) {
    
    if (const auto it = STRACTION.find(action_str); it != STRACTION.end()) {
      action_ = it->second;
    } else {
      action_ = Action::Unknown;
    }
  }

  [[nodiscard]] const std::string& target()   const noexcept { return target_; }
  [[nodiscard]] const std::string& upstream() const noexcept { return upstream_; }
  [[nodiscard]] const std::string& branch()   const noexcept { return branch_; }
  [[nodiscard]] Action action()               const noexcept { return action_; }

  friend class Repository;
};

class [[nodiscard]] Repository {
  RepoArgs args_;
  std::string hash_;
  bool hash_updated_;

  [[nodiscard]] std::expected<void, std::string>
  navigate() const noexcept {
    try {
      if (std::filesystem::exists(args_.target())) {
        std::filesystem::current_path(args_.target());
        return {};
      }
      return std::unexpected(std::format("Target path '{}' does not exist", args_.target()));
    } catch (const std::filesystem::filesystem_error& e) {
      return std::unexpected(std::format("Filesystem error: {}", e.what()));
    }
  }

  void
  migma(const std::shared_ptr<GlobalOptions>& opts) const noexcept {
    try {
      const std::filesystem::path target_path(args_.target());
      if (!std::filesystem::exists(target_path)) {
        return;
      }
      
      for (const auto& [migma_file, interpreter] : MIGMA) {
        const std::filesystem::path migma_path = target_path / migma_file;
        if (std::filesystem::exists(migma_path)) {
          const auto migma_cmd = std::format("{} {}", interpreter, migma_file);
          const auto output = safe_exec(migma_cmd, false);
          if (output) {
            if (opts->is_verbose()) {
              std::cout << *output << '\n';
            }
          } else {
            std::cout << std::format("Failed to execute migma command '{}': {}\n", migma_cmd, output.error());
          }
          break;
        }
      }
    } catch (const std::filesystem::filesystem_error& e) {
      std::cout << std::format("Migma filesystem error: {}\n", e.what());
    }
  }

  virtual void
  pull   (const std::shared_ptr<GlobalOptions>&) {}

  virtual void
  rebase (const std::shared_ptr<GlobalOptions>&) {}

  public:
  Repository(RepoArgs args, std::string hash)
    : args_(std::move(args)), hash_(std::move(hash)), hash_updated_(false) {}

  virtual ~Repository() = default;

  Repository(const Repository&)             = delete;
  Repository& operator=(const Repository&)  = delete;
  Repository(Repository&&)                  = default;
  Repository& operator=(Repository&&)       = default;

  [[nodiscard]] std::string_view
  target() const noexcept          { return args_.target();   }

  [[nodiscard]] const std::string&
  upstream() const noexcept        { return args_.upstream(); }

  [[nodiscard]] const std::string&
  branch() const noexcept          { return args_.branch();   }

  [[nodiscard]] const std::string&
  repo_hash() const noexcept       { return hash_;            }

  [[nodiscard]] bool
  is_hash_updated() const noexcept { return hash_updated_;    }

  void
  set_hash(std::string new_hash) {
    if (hash_ != new_hash) {
      std::cout << std::format("new hash: {}\n", new_hash);
      hash_ = std::move(new_hash);
      hash_updated_ = true;
    }
  }

  void
  process(const std::shared_ptr<GlobalOptions>& opts) {
    const auto nav_result = navigate();
    if (!nav_result) {
      std::cout << std::format("Navigation failed: {}\n", nav_result.error());
      return;
    }
    
    switch (args_.action()) {
      [[likely]] case Action::Pull: {
        pull(opts);
        break;
      }
      case Action::Rebase: {
        rebase(opts);
        break;
      }
      [[unlikely]] case Action::Unknown: {
        std::cout << "unknown task for" << this << std::endl;
        return;
      }
    }
    
    if (is_hash_updated()) {
      migma(opts);
    }
  }

  [[nodiscard]] std::string
  details() const {
    return std::format("{} ({}) [{}]", 
                      args_.target(), 
                      args_.branch(), 
                      action_to_string(args_.action()));
  }

  friend std::ostream& operator
  << (std::ostream& os, const Repository& r) {
    os << r.target();
    return os;
  }

  friend std::ostream& operator
  << (std::ostream& os, const Repository* r) {
    if (r) {
      os << r->target();
    } else {
      os << "null";
    }
    return os;
  }
};

template <VCS G>
class [[nodiscard]] Repo final : public Repository {
  void
  pull (const std::shared_ptr<GlobalOptions>&) override;

  void
  rebase (const std::shared_ptr<GlobalOptions>&) override;

  public:
  Repo(RepoArgs args, std::string hash)
    : Repository(std::move(args), std::move(hash)) {}
};
