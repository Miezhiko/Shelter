#pragma once

#include "utils.hpp"

#include <tuple>
#include <vector>

struct list_command {
  bool show_help = false;
  std::string directory;

  list_command(lyra::cli & cli) {
    cli.add_argument(
      lyra::command(
        "list", [this](const lyra::group & g) { this->do_command(g); })
        .help("Show repositories.")
        .add_argument(lyra::help(show_help))
        .add_argument(
          lyra::arg(directory, "directory")
            .optional()
            .help("Target directory"))
    );
  }

  [[noreturn]] void
  do_command(const lyra::group & g) {
    if (show_help) {
      std::cout << g;
      std::exit(EXIT_SUCCESS);
    }

    const std::string config_file = utils::get_config_path(std::string{CONFIG_FILE});
    if (!std::filesystem::exists(config_file)) {
      std::exit(EXIT_SUCCESS);
    }

    auto config = YAML::LoadFile(config_file);

    if (directory.empty()) {
      // Collect entries and compute max widths
      struct Entry { std::string target; std::string branch; std::string task; };
      std::vector<Entry> entries;
      entries.reserve(config.size());

      size_t max_target_len = 0;
      size_t max_branch_len = 0;

      for (const auto& node : config) {
        const auto target = node["target"].as<std::string>();
        const auto branch = node["branch"].as<std::string>();
        const auto task   = node["task"].as<std::string>();
        max_target_len = std::max(max_target_len, target.size());
        max_branch_len = std::max(max_branch_len, branch.size());
        entries.push_back({std::move(target), std::move(branch), std::move(task)});
      }

      for (const auto& e : entries) {
        std::string target_padding(max_target_len - e.target.size(), ' ');
        std::string branch_padding(max_branch_len - e.branch.size(), ' ');
        std::cout << e.target << target_padding
                  << " (" << e.branch << ")" << branch_padding
                  << " [" << e.task << "]" << std::endl;
      }
    } else {
      bool found = false;
      for (const auto& node : config) {
        const auto target_str = node["target"].as<std::string>();
        if (directory == target_str) {
          const auto task   = node["task"].as<std::string>();
          const auto branch = node["branch"].as<std::string>();
          std::cout << target_str
                    << " (" << branch << ")"
                    << " [" << task << "]" << std::endl;
          found = true;
        }
      }
      if (!found) {
        std::cout << "repository " << directory << " not found" << std::endl;
      }
    }
    std::exit(EXIT_SUCCESS);
  }
};
