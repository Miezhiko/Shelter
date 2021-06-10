#include "repository.hpp"
#include "yaml-cpp/yaml.h"

#include <iostream>
#include <filesystem>

static const char* CONFIG_FILE = "../config.yaml";

std::vector<std::shared_ptr<Repository>> parse_config(const std::string& yaml_file) {
  std::vector<std::shared_ptr<Repository>> result;
  const auto config = YAML::LoadFile(yaml_file);
  for(const auto node : config) {
    if(node["target"] && node["task"] && node["upstream"]) {
      const auto target_str   = node["target"].as<std::string>();
      const auto action_str   = node["task"].as<std::string>();
      const auto upstream_str = node["upstream"].as<std::string>();
      RepoArgs args( target_str, action_str, upstream_str );
      if(node["vcs"]) {
        const auto vcs = node["vcs"].as<std::string>();
        if (vcs == "git") {
          result.push_back(
            std::make_shared<Repo<VCS::Git>>(args)
          );
        }
        else if (vcs == "pijul") {
          result.push_back(
            std::make_shared<Repo<VCS::Pijul>>(args)
          );
        }
      } else {
        result.push_back(
          std::make_shared<Repository>(args)
        );
      }
    }
  }
  return result;
}

void process(const std::shared_ptr<Repository>& repo) {
  repo->navigate();
  repo->pull();
}

int main() {
  if (std::filesystem::exists(CONFIG_FILE)) {
    const auto repositories = parse_config(CONFIG_FILE);
    for(const auto repo : repositories) {
      std::cout << "processing: " << repo << std::endl;
      process(repo);
    }
  }
  return 0;
}
