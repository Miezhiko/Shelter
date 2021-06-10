#pragma once

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
  Action action;
  public:
  RepoArgs(std::string t, std::string a, std::string u) {
    target = t;
    upstream = u;
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
  std::string target() const {
    return args.target;
  }
  public:
  Repository(RepoArgs& a) : args(a) {};
  void navigate() const {
    std::filesystem::current_path(args.target);
  }
  std::string upstream() const {
    return args.upstream;
  }
  virtual void pull() const {};
  friend std::ostream& operator<< (std::ostream& os, const Repository& r) {
    os << r.target();
    return os;
  }
  friend std::ostream& operator<< (std::ostream& os, const Repository* r) {
    os << r->target();
    return os;
  }
};

template <VCS G>
class Repo : public Repository {
  public:
  Repo(RepoArgs& a)
    : Repository(a) {};
  virtual void pull() const;
};

template <> void Repo <VCS::Git> :: pull () const {
  std::string pull_cmd = "git pull " + upstream();
  exec(pull_cmd.c_str());
}

template <> void Repo <VCS::Pijul> :: pull () const {
  std::cout << "pijul" << std::endl;
}
