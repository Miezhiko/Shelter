#include "utils.hpp"

#include "git.hpp"
#include "pijul.hpp"

#include "yaml-cpp/yaml.h"
#include "yaml-cpp/node/node.h"

#ifdef _WIN32
#pragma warning( push )
#pragma warning ( disable : 4100 )
#pragma warning ( disable : 4458 )
#endif

#include "lyra/lyra.hpp"

#ifdef _WIN32
#pragma warning( pop )
#endif

#include <iostream>
#include <filesystem>
#include <fstream>

// You can't cout just dfine variable, you need to do #x thing
#define STRINGIFY(x) #x
// You can't just use #x thing you need to convert it into macro
// The extra level of indirection causes the value of the macro to be stringified instead of the name of the macro.
#define STRINGIFY_M(x) STRINGIFY(x)

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

void process( std::shared_ptr<Repository>& repo
            , const std::shared_ptr<GlobalOptions>& opts ) {
  if (repo->navigate()) {
    repo->process(opts);
    if (repo->is_hash_updated()) {
      repo->migma(opts);
    }
  }
}

void save_config(YAML::Node& config, const std::string& conf) {
  std::ofstream fout(conf);
  fout << config;
  fout.flush();
  std::cout << "saving config" << std::endl;
  fout.close();
}

void show_version(bool display_git_stats = false) {
  #ifdef VERSION_CMAKE
  std::cout << "Shelter v" << STRINGIFY_M(VERSION_CMAKE) << std::endl;
  #endif
  if (display_git_stats) {
    #if defined(BRANCH_CMAKE) && defined(HASH_CMAKE)
      std::cout << "Git branch: " << STRINGIFY_M(BRANCH_CMAKE)
            << ", Commit: " << STRINGIFY_M(HASH_CMAKE) <<  std::endl;
    #endif
  }
}

void list_repositories(std::vector<std::shared_ptr<Repository>>& repositories) {
  for (auto &repo : repositories) {
    std::cout << repo->details() << std::endl;
  }
}

struct add_command
{
  bool show_help = false;
  std::string directory;
  std::string action = "pull";
  std::string branch = "masterr";

  add_command(lyra::cli & cli)
  {
    cli.add_argument(
      lyra::command(
        "add", [this](const lyra::group & g) { this->do_command(g); })
        .help("Add directory.")
        .add_argument(lyra::help(show_help))
        .add_argument(
          lyra::arg(directory, "directory")
            .required()
            .help("Target directory"))
        .add_argument(
          lyra::opt(action, "action")
            .name("-t").name("--task")
            .optional()
            .help("Action type"))
        .add_argument(
          lyra::opt(branch, "branch")
            .name("-b").name("--branch")
            .optional()
            .help("Target branch"))
    );
  }

  void do_command(const lyra::group & g)
  {
    if (show_help) {
      std::cout << g;
    } else {
      const auto HomeDirectory = utils::get_home_dir();
      const std::string config_file = HomeDirectory + std::string("/") + CONFIG_FILE;
      if (std::filesystem::exists(config_file)) {
        YAML::Node result;
        auto config = YAML::LoadFile(config_file);
        result = config;
        YAML::Node new_node;
        new_node["target"] = directory;
        new_node["task"] = action;
        new_node["upstream"] = "upstream";
        new_node["branch"] = branch;
        new_node["vcs"] = "git";
        new_node["hash"] = "";
        result.push_back(new_node);
        save_config(result, config_file);
      }
    }
    exit(0);
  }
};

int main(int argc, char *argv[]) {
  auto verbose  = false;
  auto help     = false;
  auto do_exit  = false;
  auto list     = false;
  auto cli
    = lyra::cli()
    | lyra::help(help)
    | lyra::opt(verbose)
      ["-v"]["--verbose"]
      ("Display verbose output")
    | lyra::opt(list)
      ["-l"]["--list"]
      ("Show tracking repositories")
    | lyra::opt(
      [&](bool){ 
        show_version(true);
        do_exit = true;
      })
      ["--version"]
      ("Display version")
    ;

  add_command add { cli };

  const auto result = cli.parse( { argc, argv } );
  if ( !result ) {
    std::cerr << "Error in command line: " << result.message() << std::endl;
    exit(1);
  }

  if (do_exit) {
    exit(0);
  }

  show_version();

  if (help) {
    std::cout << "\n" << cli << std::endl;
    exit(0);
  }

  const auto HomeDirectory = utils::get_home_dir();

  const std::string options_file = HomeDirectory + std::string("/") + OPTIONS_FILE;
  const std::string config_file = HomeDirectory + std::string("/") + CONFIG_FILE;

  std::shared_ptr<GlobalOptions> otpions = std::make_shared<GlobalOptions>();
  
  if (std::filesystem::exists(options_file)) {
    otpions->parse_options(options_file);
  }

  if (verbose) {
    otpions->set_verbose(true);
  }

  if (std::filesystem::exists(config_file)) {
    auto config = YAML::LoadFile(config_file);
    auto repositories = parse_config(config);

    if (list) {
      list_repositories(repositories);
      return 0;
    }

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
