#pragma once

#include "yaml-cpp/yaml.h"
#include "yaml-cpp/node/node.h"

class GlobalOptions final {
  bool clean;
  bool verbose;
  public:

  GlobalOptions()
    : clean(true)
    , verbose(true) {};

  GlobalOptions( bool c
               , bool v)
    : clean(c)
    , verbose(v) {};

  bool
  do_clean() const {
    return clean;
  }

  bool
  is_verbose() const {
    return verbose;
  }

  void
  set_verbose(bool v) {
    this->verbose = v;
  }

  void
  parse_options(const std::string& yaml_file) {
    const auto& options = YAML::LoadFile(yaml_file);
    if (options["clean"]) {
      this->clean = options["clean"].as<bool>();
    }
    if (options["verbose"]) {
      this->verbose = options["verbose"].as<bool>();
    }
  }
};
