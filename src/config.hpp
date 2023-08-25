#pragma once

#include "repository.hpp"

#include "yaml-cpp/yaml.h"
#include "yaml-cpp/node/node.h"

#ifdef _WIN32
#pragma warning( push )
#pragma warning ( disable : 4100 )
#pragma warning ( disable : 4458 )
#endif

#ifdef _WIN32
#pragma warning( pop )
#endif

#include <iostream>
#include <filesystem>
#include <fstream>

static const char* OPTIONS_FILE = ".shelter_options.yml";
static const char* CONFIG_FILE  = ".shelter.yml";

const std::vector<std::shared_ptr<Repository>> parse_config(const YAML::Node& config) {
  std::vector<std::shared_ptr<Repository>> result;
  result.reserve(config.size());
  for (const auto& node : config) {
    const auto& targetNode    = node["target"];
    const auto& taskNode      = node["task"];
    const auto& upstreamNode  = node["upstream"];
    const auto& branchNode    = node["branch"];
    if (targetNode && taskNode && upstreamNode && branchNode) {
      const RepoArgs args(
        targetNode.as<std::string>(),
        taskNode.as<std::string>(),
        upstreamNode.as<std::string>(),
        branchNode.as<std::string>()
      );

      std::string hash_str;
      const auto& hashNode = node["hash"];
      if (hashNode) {
        hash_str = hashNode.as<std::string>();
      }

      if(const auto vcsNode = node["vcs"]) {
        const auto vcs = vcsNode.as<std::string>();
        if (vcs == "git") {
          result.push_back(
            std::make_shared<Repo<VCS::Git>>(args, hash_str)
          );
        } else if (vcs == "pijul") {
          result.push_back(
            std::make_shared<Repo<VCS::Pijul>>(args, hash_str)
          );
        }
      } else {
        result.push_back(
          std::make_shared<Repository>(args, hash_str)
        );
      }
    }
  }
  return result;
}

void save_config(YAML::Node& config, const std::string& conf) {
  std::ofstream fout(conf);
  fout << config;
  fout.flush();
  std::cout << "saving config" << std::endl;
  fout.close();
}
