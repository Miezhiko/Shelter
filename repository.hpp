#pragma once

#include "options.hpp"
#include "execute.hpp"

#include <iostream>
#include <filesystem>
#include <unordered_map>

enum class VCS { Git, Pijul };
enum class Action { Pull, Rebase, Unkown };
static std::unordered_map<std::string, Action> const STRACTION =
  { { "pull",    Action::Pull   }
  , { "rebase",  Action::Rebase }
  };

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
  std::string target() const { return args.target; }
  std::string upstream() const { return args.upstream; }
  std::string branch() const { return args.branch; }
  std::string repo_hash() const { return hash; }
  bool is_hash_updated() const { return hash_updated; }
  void set_hash(std::string h) {
    if (hash != h) {
      std::cout << "new hash: " << h << std::endl;
      hash = h;
      hash_updated = true;
    }
  }
  virtual void pull(const std::shared_ptr<GlobalOptions>&) {};
  virtual void rebase(const std::shared_ptr<GlobalOptions>&) {};
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
class Repo : public Repository {
  public:
  Repo(const RepoArgs& a, std::string h)
    : Repository(a, h) {};
  virtual void pull(const std::shared_ptr<GlobalOptions>&);
  virtual void rebase(const std::shared_ptr<GlobalOptions>&);
};
