# Shelter

[![Build Status](https://github.com/Miezhiko/Shelter/actions/workflows/cmake.yml/badge.svg)](https://github.com/Miezhiko/Shelter/actions/workflows/cmake.yml)
[![Discord](https://img.shields.io/discord/611822838831251466?label=Discord&color=pink)](https://discord.gg/GdzjVvD)
[![Twitter Follow](https://img.shields.io/twitter/follow/Miezhiko.svg?style=social)](https://twitter.com/Miezhiko)

Simple stupid script to update code in number of directories

Config file example, should be stored at `${HOME}/.shelter.yml`

```yaml
- branch: master
  target: /some/directory/path
  vcs: git
  upstream: origin master
  task: pull
- branch: master
  target: /another/directory
  vcs: git
  upstream: origin master
  task: pull
```

If repository contains migma files (`.migma.py` or `.migma.sh`) in root directory they will be executed if repository was updated.

Work in progress
================

currently only git version control system is supported for good

```cpp
template <> void Repo <VCS::Git> :: pull (
  const std::shared_ptr<GlobalOptions>& opts
) {
  const auto repo_branch = branch();
  if (get_branch() != repo_branch) {
    std::string checkout_cmd = "git checkout " + repo_branch;
    exec(checkout_cmd.c_str());
  }

  std::string local_hash = repo_hash();
  if (local_hash.empty()) {
    local_hash = get_local_hash();
    set_hash( local_hash );
  }

  const auto repo_upstream = upstream();
  const auto remote_hash = get_remote_hash( repo_upstream );

  if (local_hash == remote_hash) {
    std::cout << "repository " << this << " is up to date" << std::endl;
    return;
  }

  if (opts->do_clean()) {
    exec("git reset --hard");
    exec("git clean -fxd");
  }

  std::string pull_cmd = "git pull " + repo_upstream;
  exec(pull_cmd.c_str());

  set_hash( remote_hash );
}
```
