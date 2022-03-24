#pragma once

#include "options.hpp"
#include "execute.hpp"

#include <filesystem>
#include <unordered_map>

enum class VCS { Git, Pijul };
enum class Action { Pull, Rebase, Unkown };

static std::unordered_map<std::string, Action> const STRACTION =
  { { "pull",    Action::Pull   }
  , { "rebase",  Action::Rebase }
  };

static std::unordered_map<std::string, std::string> const MIGMA =
  { { ".migma.py",    "python" }
  , { ".migma.sh",    "bash" }
  , { ".migma.raku",  "raku" }
  , { ".migma.pl",    "perl" }
  };

std::ostream& operator << (std::ostream& os, const Action& a) {
  auto it = std::find_if( std::begin(STRACTION), std::end(STRACTION)
                        , [&a](auto&& p) { return p.second == a; } );

  if (it == std::end(STRACTION)) {
    os << "Unknown ("
       << static_cast<std::underlying_type<Action>::type>(a)
       << ")";
  } else {
    os << it->first;
  }

  return os;
}

class RepoArgs {
  std::string target;
  std::string upstream;
  std::string branch;
  Action action;
  public:
  RepoArgs(std::string t, std::string a, std::string u, std::string b)
    : target(t), upstream(u), branch(b) {
    const auto it = STRACTION.find(a);
    if (it != STRACTION.end()) {
      action = it->second;
    } else {
      action = Action::Unkown;
    }
  }
  friend class Repository;
};

class Repository {
  RepoArgs args;
  std::string hash;
  bool hash_updated;
  public:
  Repository(const RepoArgs& a, std::string h)
    : args(a), hash(h), hash_updated(false) {};
  bool navigate() const {
    if (std::filesystem::exists(args.target)) {
      std::filesystem::current_path(args.target);
      return true;
    }
    return false;
  }
  const std::string_view target() const { return args.target;   }
  const std::string upstream() const    { return args.upstream; }
  const std::string branch() const      { return args.branch;   }
  const std::string repo_hash() const   { return hash;          }
  bool is_hash_updated() const          { return hash_updated;  }
  void set_hash(std::string h) {
    if (hash != h) {
      std::cout << "new hash: " << h << std::endl;
      hash = h;
      hash_updated = true;
    }
  }
  virtual void pull   (const std::shared_ptr<GlobalOptions>&) {};
  virtual void rebase (const std::shared_ptr<GlobalOptions>&) {};
  void process(const std::shared_ptr<GlobalOptions>& opts) {
    switch (args.action) {
      case Action::Pull: {
        pull(opts);
        break;
      }
      case Action::Rebase: {
        rebase(opts);
        break;
      }
      case Action::Unkown: {
        std::cout << "unknown task for " << this << std::endl;
        break;
      }
    }
  }
  void migma(const std::shared_ptr<GlobalOptions>& opts) const {
    if (std::filesystem::exists(args.target)) {
      for ( const auto &m : MIGMA ) {
        const std::string migma_file = args.target + std::string("/") + m.first;
        if (std::filesystem::exists(migma_file)) {
          const std::string migma_cmd = m.second + std::string(" ") + m.first;
          const auto output = exec(migma_cmd.c_str());
          if (opts->is_verbose()) {
            std::cout << output << std::endl;
          }
          break;
        }
      }
    }
  }
  const std::string details() {
    std::stringstream details;
    details << args.target
            << " (" << args.branch
            << ") [" << args.action << "]";
    return details.str();
  }
  friend std::ostream& operator<< (std::ostream& os, const Repository& r) {
    os << r.target();
    return os;
  }
  friend std::ostream& operator<< (std::ostream& os, const Repository* r) {
    os << r->target();
    return os;
  }
  virtual ~Repository() {};
};

template <VCS G>
class Repo final : public Repository {
  public:
  Repo(const RepoArgs& a, std::string h)
    : Repository(a, h) {};
  virtual void pull   (const std::shared_ptr<GlobalOptions>&);
  virtual void rebase (const std::shared_ptr<GlobalOptions>&);
};
