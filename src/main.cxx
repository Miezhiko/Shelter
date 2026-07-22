#include "utils.hpp"
#include "config.hpp"

#ifndef _WIN32
#include "vcs/libgit.hpp"
#endif
#include "vcs/gitshell.hpp"
#include "vcs/pijul.hpp"

#include "lyra/lyra.hpp"

#include "commands/list.hpp"
#include "commands/add.hpp"
#include "commands/rm.hpp"

#include <thread>
#include <semaphore>
#include <algorithm>

#define STRINGIFY(x) #x
#define STRINGIFY_M(x) STRINGIFY(x)

void
show_version(const bool display_git_stats = false) {
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

int
main(int argc, char *argv[]) {
  auto verbose  = false;
  auto help     = false;
  auto version  = false;
  auto cli
    = lyra::cli()
    | lyra::help(help)
    | lyra::opt(verbose)
      ["-v"]["--verbose"]
      ("Display verbose output")
    | lyra::opt(
      [&](bool){
        version = true;
      })
      ["--version"]
      ("Display version")
    ;

  list_command _list { cli };
  add_command _add { cli };
  rm_command _rm { cli };

  const auto result = cli.parse( { argc, argv } );
  if ( !result ) {
    std::cerr << "Error in command line: " << result.message() << std::endl;
    return 1;
  }

  show_version(version);
  if (version) {
    return EXIT_SUCCESS;
  }

  if (help) {
    std::cout << "\n" << cli << std::endl;
    return EXIT_SUCCESS;
  }

  const std::string config_file = utils::get_config_path(std::string{CONFIG_FILE});
  const std::string options_file = utils::get_config_path(std::string{OPTIONS_FILE});

  auto options = std::make_shared<GlobalOptions>();

  if (std::filesystem::exists(options_file)) {
    options->parse_options(options_file);
  }

  if (verbose) {
    options->set_verbose(true);
  }

  if (!std::filesystem::exists(config_file)) {
    std::cout << "missing config: " << config_file << std::endl;
    return EXIT_SUCCESS;
  }

  auto config = YAML::LoadFile(config_file);
  const auto repositories = parse_config(config);

  #ifndef _WIN32
  // Initialize libgit2 once for the whole run rather than per repository;
  // git_libgit2_init/shutdown are ref-counted and thread-safe, but doing
  // this once avoids needless lock contention when repos run in parallel.
  const GitLibGuard lib_guard;
  #endif

  const auto worker_count = std::max<size_t>(1,
    std::min<size_t>(repositories.size(), std::thread::hardware_concurrency()));
  std::counting_semaphore<> slots(static_cast<std::ptrdiff_t>(worker_count));

  std::vector<std::jthread> workers;
  workers.reserve(repositories.size());

  for (const auto& parsed : repositories) {
    slots.acquire();
    const ParsedRepo* item = &parsed; // stable pointer into `repositories`, safe to capture by value
    workers.emplace_back([&slots, item, &options] {
      sync_cout() << "processing: " << *item->repo << std::endl;
      item->repo->process(options);
      slots.release();
    });
  }
  workers.clear(); // joins all jthreads

  bool some_hash_was_updated = false;
  for (const auto& parsed : repositories) {
    if (parsed.repo->is_hash_updated()) {
      config[parsed.config_index]["hash"] = parsed.repo->repo_hash();
      some_hash_was_updated = true;
    }
  }

  if (some_hash_was_updated) {
    save_config(config, config_file);
  }

  return EXIT_SUCCESS;
}
