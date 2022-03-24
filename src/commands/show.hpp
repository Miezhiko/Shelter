#pragma once

struct show_command
{
  bool show_help = false;
  std::string directory;

  show_command(lyra::cli & cli)
  {
    cli.add_argument(
      lyra::command(
        "show", [this](const lyra::group & g) { this->do_command(g); })
        .help("Show repositories.")
        .add_argument(lyra::help(show_help))
        .add_argument(
          lyra::arg(directory, "directory")
            .optional()
            .help("Target directory"))
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
        auto config = YAML::LoadFile(config_file);
        if (directory.empty()) {
          for(YAML::Node node : config) {
            const auto target_str = node["target"].as<std::string>();
            const auto task_str   = node["task"]  .as<std::string>();
            const auto branch_str = node["branch"].as<std::string>();
            std::cout << target_str
                      << " (" << branch_str << ") "
                      << " [" << task_str << "]" << std::endl;
          }
        } else {
          bool found = false;
          for(YAML::Node node : config) {
            const auto target_str = node["target"].as<std::string>();
            if (directory == target_str) {
              const auto task_str   = node["task"]  .as<std::string>();
              const auto branch_str = node["branch"].as<std::string>();
              std::cout << target_str
                        << " (" << branch_str << ") "
                        << " [" << task_str << "]" << std::endl;
              found = true;
            }
          }
          if (!found) {
            std::cout << "repository " << directory << " not found" << std::endl;
          }
        }
      }
    }
    exit(0);
  }
};
