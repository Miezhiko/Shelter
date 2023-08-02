# Shelter

[![Build Status](https://github.com/Miezhiko/Shelter/actions/workflows/cmake.yml/badge.svg)](https://github.com/Miezhiko/Shelter/actions/workflows/cmake.yml)
[![Discord](https://img.shields.io/discord/611822838831251466?label=Discord&color=pink)](https://discord.gg/GdzjVvD)
[![Twitter Follow](https://img.shields.io/twitter/follow/Miezhiko.svg?style=social)](https://twitter.com/Miezhiko)

Simple stupid script to update code in number of directories

Wants/uses `libgit2` if it's not Windows, which makes it fast, actually.

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
  ...
}
```
