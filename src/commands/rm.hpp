#pragma once

#include "utils.hpp"

struct rm_command {
  bool show_help = false;
  std::string directory;

  rm_command(lyra::cli & cli) {
    cli.add_argument(
      lyra::command(
        "rm", [this](const lyra::group & g) { this->do_command(g); })
        .help("Remove directory.")
        .add_argument(lyra::help(show_help))
        .add_argument(
          lyra::arg(directory, "directory")
            .optional()
            .help("Target directory"))
    );
  }

  [[noreturn]] void
  do_command(const lyra::group & g) {
    if (directory.empty() || directory == ".") {
      directory = std::filesystem::current_path().generic_string();
    }
    if (show_help) {
      std::cout << g;
      std::exit(EXIT_SUCCESS);
    }

    const std::string config_file = utils::get_config_path(std::string{CONFIG_FILE});
    if (!std::filesystem::exists(config_file)) {
      std::exit(EXIT_SUCCESS);
    }

    auto config = YAML::LoadFile(config_file);
    unsigned int node_index = 0;
    for (const auto& node : config) {
      const auto& target_str = node["target"].as<std::string>();
      if (target_str == directory) {
        if (config.remove(node_index)) {
          save_config(config, config_file);
          std::exit(EXIT_SUCCESS);
        } else {
          std::cout << "failed to remove repository " << directory << std::endl;
          std::exit(EXIT_FAILURE);
        }
      }
      node_index++;
    }
    std::cout << "repository " << directory << " not found" << std::endl;
    std::exit(EXIT_SUCCESS);
  }
};
