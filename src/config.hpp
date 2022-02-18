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
static const char* CONFIG_FILE = ".shelter.yml";

const std::vector<std::shared_ptr<Repository>> parse_config(const YAML::Node& config) {
  std::vector<std::shared_ptr<Repository>> result;
  for (const auto node : config) {
    if (node["target"] && node["task"] && node["upstream"] && node["branch"]) {
      const auto target_str   = node["target"].as<std::string>();
      const auto action_str   = node["task"].as<std::string>();
      const auto upstream_str = node["upstream"].as<std::string>();
      const auto branch_str   = node["branch"].as<std::string>();
      const RepoArgs args( target_str, action_str, upstream_str, branch_str );

      std::string hash_str;
      if (node["hash"]) {
        hash_str = node["hash"].as<std::string>();
      }

      if(node["vcs"]) {
        const auto vcs = node["vcs"].as<std::string>();
        if (vcs == "git") {
          result.push_back(
            std::make_shared<Repo<VCS::Git>>(args, hash_str)
          );
        }
        else if (vcs == "pijul") {
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
