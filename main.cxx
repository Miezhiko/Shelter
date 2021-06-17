#include "repository.hpp"

#include "git.hpp"
#include "pijul.hpp"

#include "yaml-cpp/yaml.h"
#include "yaml-cpp/node/node.h"

#include <iostream>
#include <filesystem>
#include <fstream>

static const char* OPTIONS_FILE = ".shelter_options.yml";
static const char* CONFIG_FILE = ".shelter.yml";

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

  #ifdef unix
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wpedantic"
  #pragma GCC diagnostic ignored "-Wnarrowing"
    const static volatile char A = 'a';
    const char HOME[5] = {A-25, A-18, A-20, A-28, 0};
    auto HomeDirectory = getenv(HOME);
  #pragma GCC diagnostic pop
  #elif defined(_WIN32)
    char* HomeDirectory;
    size_t required_size;
    getenv_s( &required_size, NULL, 0, "USERPROFILE");
    if (required_size == 0) {
      std::cout << "USERPROFILE env doesn't exist!" << std::endl;
      return 1;
    }
    HomeDirectory = (char*) malloc(required_size * sizeof(char));
    if (!HomeDirectory) {
      std::cout <<("Failed to allocate memory!\n");
      return 1;
    }
    getenv_s( &required_size, HomeDirectory, required_size, "USERPROFILE" );
  #else
    auto HomeDirectory = ".";
  #endif

  std::cout << "home dir: " << HomeDirectory << std::endl;

  const std::string options_file = HomeDirectory + std::string("/") + OPTIONS_FILE;
  const std::string config_file = HomeDirectory + std::string("/") + CONFIG_FILE;

  std::shared_ptr<GlobalOptions> otpions;
  if (std::filesystem::exists(options_file)) {
    otpions = parse_options(options_file);
  } else {
    otpions = std::make_shared<GlobalOptions>();
  }

  if (std::filesystem::exists(config_file)) {
    auto config = YAML::LoadFile(config_file);
    auto repositories = parse_config(config);

    bool some_hash_was_updated = false;

    for (auto &repo : repositories) {
      std::cout << "processing: " << repo << std::endl;
      process(repo, otpions);
      if (repo->is_hash_updated()) {
        for (YAML::iterator it = config.begin(); it != config.end(); ++it) {
          const YAML::Node& node = *it;
          if (node["target"]) {
            const auto target_str = node["target"].as<std::string>();
            if (target_str == repo->target()) {
              (*it)["hash"] = repo->repo_hash();
              if (!some_hash_was_updated) {
                some_hash_was_updated = true;
              }
            }
          }
        }
      }
    }

    if (some_hash_was_updated) {
      save_config(config, config_file);
    }
  } else {
    std::cout << "missing config: " << config_file << std::endl;
  }

  return 0;
}
