# Shelter

[![Build Status](https://github.com/Qeenon/Shelter/actions/workflows/cmake.yml/badge.svg)](https://github.com/Qeenon/Shelter/actions/workflows/cmake.yml)
[![Discord](https://img.shields.io/discord/611822838831251466?label=Discord&color=pink)](https://discord.gg/GdzjVvD)
[![Twitter Follow](https://img.shields.io/twitter/follow/Qeenon.svg?style=social)](https://twitter.com/Qeenon)

Simple stupid script to update code in number of directories

```cpp
template <> void Repo <VCS::Git> :: pull (
  const std::shared_ptr<GlobalOptions>& opts
) {
  if (get_branch() != branch()) {
    std::string checkout_cmd = "git checkout " + branch();
    exec(checkout_cmd.c_str());
  }

  if (get_hash() == repo_hash()) {
    std::cout << "repository " << this << " is up to date" << std::endl;
    return;
  }

  if (opts->do_clean()) {
    exec("git reset --hard");
    exec("git clean -fxd");
  }

  std::string pull_cmd = "git pull " + upstream();
  exec(pull_cmd.c_str());

  set_hash( get_hash() );
}
```

config file example, should be stored at `${HOME}/.shelter.yml`

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
