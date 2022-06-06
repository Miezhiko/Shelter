#include "utils.hpp"
#include "config.hpp"

#include "vcs/git.hpp"
#include "vcs/pijul.hpp"

#include "lyra/lyra.hpp"

#include "commands/show.hpp"
#include "commands/add.hpp"
#include "commands/rm.hpp"

// You can't cout just dfine variable, you need to do #x thing
#define STRINGIFY(x) #x
// You can't just use #x thing you need to convert it into macro
// The extra level of indirection causes the value of the macro to be stringified instead of the name of the macro.
#define STRINGIFY_M(x) STRINGIFY(x)

void show_version(bool display_git_stats = false) {
  #ifdef VERSION_CMAKE
  std::cout << "Shelter v" << STRINGIFY_M(VERSION_CMAKE) << std::endl;
  #endif
  if (display_git_stats) {
    #if defined(BRANCH_CMAKE) && defined(HASH_CMAKE)
      std::cout << "Git branch: " << STRINGIFY_M(BRANCH_CMAKE)
                << ", Commit: "   << STRINGIFY_M(HASH_CMAKE)
                <<  std::endl;
    #endif
  }
}

void list_repositories(std::vector<std::shared_ptr<Repository>>& repositories) {
  for (auto &repo : repositories) {
    std::cout << repo->details() << std::endl;
  }
}

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

  show_command _show { cli };
  add_command _add { cli };
  rm_command _rm { cli };

  const auto result = cli.parse( { argc, argv } );
  if ( !result ) {
    std::cerr << "Error in command line: " << result.message() << std::endl;
    return 1;
  }

  if (do_exit) {
    return 0;
  }

  show_version();

  if (help) {
    std::cout << "\n" << cli << std::endl;
    return 0;
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
      repo->process(otpions);
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
