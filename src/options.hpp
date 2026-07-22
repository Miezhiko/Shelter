#pragma once

#include "yaml-cpp/yaml.h"
#include "yaml-cpp/node/node.h"

class [[nodiscard]] GlobalOptions final {
  bool clean;
  bool force;
  bool verbose;
  public:

  GlobalOptions()
    : clean(true)
    , force(false)
    , verbose(false) {};

  GlobalOptions( bool c
               , bool f
               , bool v)
    : clean(c)
    , force(f)
    , verbose(v) {};

  [[nodiscard]] bool
  do_clean() const {
    return clean;
  }

  [[nodiscard]] bool
  do_force() const {
    return force;
  }

  [[nodiscard]] bool
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
    if (options["force"]) {
      this->force = options["force"].as<bool>();
    }
    if (options["verbose"]) {
      this->verbose = options["verbose"].as<bool>();
    }
  }
};
