namespace {
  std::string get_branch() {
    std::string rev_parse = exec("git rev-parse --abbrev-ref HEAD");
    rev_parse.erase(std::remove(rev_parse.begin(), rev_parse.end(), '\n'), rev_parse.end());
    return rev_parse;
  }
}

template <> void Repo <VCS::Git> :: pull (
  const std::shared_ptr<GlobalOptions>& opts
) const {
  if (get_branch() != branch()) {
    std::string checkout_cmd = "git checkout " + branch();
    exec(checkout_cmd.c_str());
  }

  if (opts->do_clean()) {
    exec("git clean -fxd");
  }

  std::string pull_cmd = "git pull " + upstream();
  exec(pull_cmd.c_str());
}

template <> void Repo <VCS::Git> :: rebase (
  const std::shared_ptr<GlobalOptions>& opts
) const {
  if (get_branch() != branch()) {
    std::string checkout_cmd = "git checkout " + branch();
    exec(checkout_cmd.c_str());
  }

  if (opts->do_clean()) {
    exec("git clean -fxd");
  }

  std::string pull_cmd = "git pull --rebase " + upstream();
  exec(pull_cmd.c_str());
}
