#include "repository.hpp"

#include "git.hpp"
#include "pijul.hpp"

#include "yaml-cpp/yaml.h"

#include <iostream>
#include <filesystem>

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

std::vector<std::shared_ptr<Repository>> parse_config(const std::string& yaml_file) {
  std::vector<std::shared_ptr<Repository>> result;
  const auto config = YAML::LoadFile(yaml_file);
  for (const auto node : config) {
    if (node["target"] && node["task"] && node["upstream"] && node["branch"]) {
      const auto target_str   = node["target"].as<std::string>();
      const auto action_str   = node["task"].as<std::string>();
      const auto upstream_str = node["upstream"].as<std::string>();
      const auto branch_str   = node["branch"].as<std::string>();
      RepoArgs args( target_str, action_str, upstream_str, branch_str );
      if (node["hash"]) {
        const auto hash_str   = node["hash"].as<std::string>();
        args.set_hash( hash_str );
      }
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

void process( const std::shared_ptr<Repository>& repo
            , const std::shared_ptr<GlobalOptions>& opts ) {
  if (repo->navigate()) {
    repo->process(opts);
  }
}

int main() {
  std::shared_ptr<GlobalOptions> otpions;
  if (std::filesystem::exists(OPTIONS_FILE)) {
    otpions = parse_options(OPTIONS_FILE);
  } else {
    otpions = std::make_shared<GlobalOptions>();
  }
  if (std::filesystem::exists(CONFIG_FILE)) {
    const auto repositories = parse_config(CONFIG_FILE);
    for (const auto &repo : repositories) {
      std::cout << "processing: " << repo << std::endl;
      process(repo, otpions);
    }
  }
  return 0;
}
