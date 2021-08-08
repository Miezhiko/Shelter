#pragma once

#include "yaml-cpp/yaml.h"
#include "yaml-cpp/node/node.h"

class GlobalOptions {
  bool clean;
  bool verbose;
  public:
  bool do_clean() const {
    return clean;
  }
  bool is_verbose() const {
    return verbose;
  }
  void set_verbose(bool v) {
    this->verbose = v;
  }
  void parse_options(const std::string& yaml_file) {
    const auto options = YAML::LoadFile(yaml_file);
    auto global_options = std::make_shared<GlobalOptions>();
    if (options["clean"]) {
      const auto clean_bool = options["clean"].as<bool>();
      this->clean = clean_bool;
    }
    if (options["verbose"]) {
      const auto verbose_bool = options["verbose"].as<bool>();
      this->verbose = verbose_bool;
    }
  }
  GlobalOptions()
    : clean(true)
    , verbose(true) {};
  GlobalOptions( bool c
               , bool v)
    : clean(c)
    , verbose(v) {};
};
