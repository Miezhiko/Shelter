#pragma once

#include "utils.hpp"
#include "vcs/gitshell.hpp"

struct add_command {
  bool show_help        = false;
  std::string branch    {};
  std::string upstream  {};
  std::string remote    {};
  std::string directory {};
  std::string action    = "pull";
  std::string vcs       = "git";

  add_command(lyra::cli & cli) {
    cli.add_argument(
      lyra::command(
        "add", [this](const lyra::group & g) { this->do_command(g); })
        .help("Add directory.")
        .add_argument(lyra::help(show_help))
        .add_argument(
          lyra::arg(directory, "directory")
            .optional()
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
        .add_argument(
          lyra::opt(upstream, "upstream")
            .name("-u").name("--upstream")
            .optional()
            .help("Target upstream"))
        .add_argument(
          lyra::opt(remote, "remote")
            .name("-r").name("--remote")
            .optional()
            .help("Target remote"))
        .add_argument(
          lyra::opt(vcs, "vcs")
            .name("--vcs")
            .optional()
            .help("Target version control system (git)"))
    );
  }

  [[noreturn]] void
  do_command(const lyra::group & g) {
    if (directory.empty() || directory == ".") {
      directory = std::filesystem::current_path().generic_string();
    }
    if (branch.empty()) {
      if (auto branchShell = gitshell::get_branch()) {
        branch = branchShell.value();
      } else {
        branch = "master";
      }
    }
    if (upstream.empty()) {
      upstream = "origin " + branch;
    }

    if (show_help) {
      std::cout << g;
    } else {
      const std::string config_file = utils::get_config_path(std::string{CONFIG_FILE});
      if (std::filesystem::exists(config_file)) {
        auto config = YAML::LoadFile(config_file);
        YAML::Node new_node;
        new_node["target"]    = directory;
        new_node["task"]      = action;
        new_node["upstream"]  = upstream;
        new_node["branch"]    = branch;
        new_node["vcs"]       = vcs;
        new_node["hash"]      = "";
        if (!remote.empty()) {
          new_node["remote"]  = remote;
        }
        config.push_back(new_node);
        save_config(config, config_file);
      }
    }
    std::exit(EXIT_SUCCESS);
  }
};
