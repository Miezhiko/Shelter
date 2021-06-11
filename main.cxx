#include "repository.hpp"

#include "git.hpp"
#include "pijul.hpp"

#include "yaml-cpp/yaml.h"
#include "yaml-cpp/node/node.h"

#include <iostream>
#include <filesystem>
#include <fstream>

static const char* OPTIONS_FILE = "../shelter.yaml";
static const char* CONFIG_FILE = "../config.yaml";

std::shared_ptr<GlobalOptions> parse_options(const std::string& yaml_file) {
  const auto options = YAML::LoadFile(yaml_file);
  if (options["clean"]) {
    const auto clean_bool = options["clean"].as<bool>();
    return std::make_shared<GlobalOptions>(clean_bool);
  }
  return std::make_shared<GlobalOptions>();
}

std::vector<std::shared_ptr<Repository>> parse_config(const YAML::Node& config) {
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

void process( std::shared_ptr<Repository>& repo
            , const std::shared_ptr<GlobalOptions>& opts ) {
  if (repo->navigate()) {
    repo->process(opts);
  }
}

void save_config(YAML::Node& config, const std::string& conf) {
  std::ofstream fout(conf);
  fout << config;
  fout.flush();
  std::cout << "saving config" << std::endl;
  fout.close();
}

int main() {
  std::shared_ptr<GlobalOptions> otpions;
  if (std::filesystem::exists(OPTIONS_FILE)) {
    otpions = parse_options(OPTIONS_FILE);
  } else {
    otpions = std::make_shared<GlobalOptions>();
  }
  if (std::filesystem::exists(CONFIG_FILE)) {
    auto config = YAML::LoadFile(CONFIG_FILE);
    auto repositories = parse_config(config);
    const auto cwd = std::filesystem::current_path();
    for (auto &repo : repositories) {
      std::cout << "processing: " << repo << std::endl;
      process(repo, otpions);
      // Update hash
      for (YAML::iterator it = config.begin(); it != config.end(); ++it) {
        const YAML::Node& node = *it;
        if (node["target"] && node["task"] && node["upstream"] && node["branch"]) {
          const auto target_str = node["target"].as<std::string>();
          if (target_str == repo->target()) {
            (*it)["hash"] = repo->repo_hash();
          }
        }
      }
    }
    std::filesystem::current_path(cwd);
    save_config(config, CONFIG_FILE);
  }
  return 0;
}
