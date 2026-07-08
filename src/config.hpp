#pragma once

#include "repository.hpp"

#include "yaml-cpp/yaml.h"

#include <iostream>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <functional>

namespace {
  template <VCS T>
  std::shared_ptr<Repository>
  makeRepo(RepoArgs args, std::string hash_str) {
    return std::make_shared<Repo<T>>(std::move(args), std::move(hash_str));
  }

  using RepoFactory = std::function<std::shared_ptr<Repository>(RepoArgs, std::string)>;
  const std::unordered_map<std::string_view, RepoFactory> VCSTYPE = {
    { "git",        [](RepoArgs a, std::string h) { return makeRepo<VCS::Git>(std::move(a), std::move(h)); } },
    { "pijul",      [](RepoArgs a, std::string h) { return makeRepo<VCS::Pijul>(std::move(a), std::move(h)); } },
    { "git shell",  [](RepoArgs a, std::string h) { return makeRepo<VCS::GitShell>(std::move(a), std::move(h)); } }
  };
}

[[nodiscard]] std::vector<std::shared_ptr<Repository>>
parse_config(const YAML::Node& config) {
  std::vector<std::shared_ptr<Repository>> result;
  result.reserve(config.size());

  for (const auto& node : config) {
    const auto& targetNode   = node["target"];
    const auto& taskNode     = node["task"];
    const auto& upstreamNode = node["upstream"];
    const auto& branchNode   = node["branch"];

    if (!targetNode || !taskNode || !upstreamNode || !branchNode) {
      continue;
    }

    std::string remote;
    if (const auto& remoteNode = node["remote"]) {
      remote = remoteNode.as<std::string>();
    }

    const RepoArgs args(
      targetNode.as<std::string>(),
      taskNode.as<std::string>(),
      upstreamNode.as<std::string>(),
      branchNode.as<std::string>(),
      std::move(remote)
    );

    std::string hash_str;
    if (const auto& hashNode = node["hash"]) {
      hash_str = hashNode.as<std::string>();
    }

    std::string vcs = "git";
    if (const auto& vcsNode = node["vcs"]) {
      vcs = vcsNode.as<std::string>();
    }

    if (const auto it = VCSTYPE.find(vcs); it != VCSTYPE.end()) {
      result.push_back(it->second(args, hash_str));
    } else {
      std::cout << "unknown vcs specified: " << vcs << ", ignoring" << std::endl;
    }
  }

  return result;
}

void
save_config(const YAML::Node& config, const std::string& path) {
  std::ofstream fout(path);
  if (!fout) {
    std::cerr << std::format("Failed to open config file for writing: {}\n", path);
    return;
  }
  fout << config;
  fout.close();
  std::cout << "config saved" << std::endl;
}
