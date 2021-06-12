namespace {
  std::string get_branch() {
    std::string rev_parse = exec("git rev-parse --abbrev-ref HEAD");
    rev_parse.erase(std::remove(rev_parse.begin(), rev_parse.end(), '\n'), rev_parse.end());
    return rev_parse;
  }
  std::string get_local_hash() {
    std::string log_hash = exec("git log -n 1 --pretty=format:%H");
    log_hash.erase(std::remove(log_hash.begin(), log_hash.end(), '\n'), log_hash.end());
    return log_hash;
  }
  std::string get_remote_hash(const std::string& upstream) {
    const std::string ls_remote_cmd = "git ls-remote " + upstream;
    std::string ls_remote = exec(ls_remote_cmd.c_str());
    std::string::size_type tpos = ls_remote.find('\t');
    if (tpos != std::string::npos) {
      return ls_remote.substr(0, tpos);
    }
    return ls_remote;
  }
}

template <> void Repo <VCS::Git> :: pull (
  const std::shared_ptr<GlobalOptions>& opts
) {
  const auto repo_branch = branch();
  if (get_branch() != repo_branch) {
    std::string checkout_cmd = "git checkout " + repo_branch;
    exec(checkout_cmd.c_str());
  }

  std::string local_hash = repo_hash();
  if (local_hash.empty()) {
    local_hash = get_local_hash();
  }

  const auto repo_upstream = upstream();
  const auto remote_hash = get_remote_hash( repo_upstream );

  if (local_hash == remote_hash) {
    std::cout << "repository " << this << " is up to date" << std::endl;
    return;
  }

  if (opts->do_clean()) {
    exec("git reset --hard");
    exec("git clean -fxd");
  }

  std::string pull_cmd = "git pull " + repo_upstream;
  exec(pull_cmd.c_str());

  set_hash( remote_hash );
}

template <> void Repo <VCS::Git> :: rebase (
  const std::shared_ptr<GlobalOptions>& opts
) {
  const auto repo_branch = branch();
  if (get_branch() != repo_branch) {
    std::string checkout_cmd = "git checkout " + repo_branch;
    exec(checkout_cmd.c_str());
  }

  std::string local_hash = repo_hash();
  if (local_hash.empty()) {
    local_hash = get_local_hash();
  }

  const auto repo_upstream = upstream();
  const auto remote_hash = get_remote_hash( repo_upstream );

  if (local_hash == remote_hash) {
    std::cout << "repository " << this << " is up to date" << std::endl;
    return;
  }

  if (opts->do_clean()) {
    exec("git reset --hard");
    exec("git clean -fxd");
  }

  std::string pull_cmd = "git pull --rebase " + repo_upstream;
  exec(pull_cmd.c_str());

  std::string push_cmd = "git push --force origin " + repo_branch;
  exec(push_cmd.c_str());

  set_hash( remote_hash );
}
