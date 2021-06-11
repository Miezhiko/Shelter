#pragma once

class GlobalOptions {
  bool clean;
  public:
  bool do_clean() const {
    return clean;
  }
  GlobalOptions()
    : clean(true) {};
  GlobalOptions(bool c)
    : clean(c) {};
};
