#pragma once

struct rm_command
{
  bool show_help = false;
  std::string directory;

  rm_command(lyra::cli & cli)
  {
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

  void do_command(const lyra::group & g)
  {
    if (directory.empty() || directory == ".") {
      directory = std::filesystem::current_path();
    }
    if (show_help) {
      std::cout << g;
    } else {
      const auto HomeDirectory = utils::get_home_dir();
      const std::string config_file = HomeDirectory + std::string("/") + CONFIG_FILE;
      if (std::filesystem::exists(config_file)) {
        auto config = YAML::LoadFile(config_file);
        for(YAML::Node node : config) {
          const auto target_str = node["target"].as<std::string>();
          if (target_str == directory) {
            config.remove(node);
            save_config(config, config_file);
          }
        }
        std::cout << "repository " << directory << " not found" << std::endl;
      }
    }
    exit(0);
  }
};
