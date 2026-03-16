#pragma once

#include <git2.h>
#include <ranges>
#include <string_view>
#include <memory>
#include <optional>
#include <expected>
#include <format>

#include "repository.hpp"

#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

class GitLibGuard final {
  public:
  GitLibGuard() { git_libgit2_init(); }
  ~GitLibGuard() { git_libgit2_shutdown(); }

  GitLibGuard(const GitLibGuard&)             = delete;
  GitLibGuard& operator=(const GitLibGuard&)  = delete;
  GitLibGuard(GitLibGuard&&)                  = delete;
  GitLibGuard& operator=(GitLibGuard&&)       = delete;
};

class GitRepoGuard final {
  public:
  explicit GitRepoGuard(git_repository* repo = nullptr) noexcept : repo_(repo) {}

  ~GitRepoGuard() noexcept {
    if (repo_) {
      git_repository_state_cleanup(repo_);
      git_repository_free(repo_);
    }
  }

  GitRepoGuard(const GitRepoGuard&)             = delete;
  GitRepoGuard& operator=(const GitRepoGuard&)  = delete;
  GitRepoGuard(GitRepoGuard&& other) noexcept : repo_(std::exchange(other.repo_, nullptr)) {}
  GitRepoGuard& operator=(GitRepoGuard&& other) noexcept {
    if (this != &other) {
      std::swap(repo_, other.repo_);
    }
    return *this;
  }

  git_repository* get() const noexcept { return repo_; }
  explicit operator bool() const noexcept { return repo_ != nullptr; }

  private:
  git_repository* repo_;
};

using GitResult       = std::expected<void, std::string>;
using GitStringResult = std::expected<std::string, std::string>;

namespace {
  [[nodiscard]] constexpr std::string_view get_git_error() noexcept {
    const auto* error = git_error_last();
    return error ? std::string_view{error->message} : "Unknown git error";
  }

  template<typename T, void(*Deleter)(T*)>
  class GitResource {
  public:
    explicit GitResource(T* resource = nullptr) noexcept : resource_(resource) {}
    ~GitResource() noexcept { if (resource_) Deleter(resource_); }

    GitResource(const GitResource&) = delete;
    GitResource& operator=(const GitResource&) = delete;
    GitResource(GitResource&& other) noexcept : resource_(std::exchange(other.resource_, nullptr)) {}
    GitResource& operator=(GitResource&& other) noexcept {
      if (this != &other) {
        std::swap(resource_, other.resource_);
      }
      return *this;
    }

    T* get() const noexcept { return resource_; }
    T** address() noexcept { return &resource_; }
    explicit operator bool() const noexcept { return resource_ != nullptr; }
    T* release() noexcept { return std::exchange(resource_, nullptr); }

  private:
    T* resource_;
  };

  using StatusList      = GitResource<git_status_list, git_status_list_free>;
  using Index           = GitResource<git_index, git_index_free>;
  using Reference       = GitResource<git_reference, git_reference_free>;
  using Remote          = GitResource<git_remote, git_remote_free>;
  using Object          = GitResource<git_object, git_object_free>;
  using AnnotatedCommit = GitResource<git_annotated_commit, git_annotated_commit_free>;

  [[nodiscard]] GitResult
  clean_repository(git_repository* repo) noexcept {
    git_status_options status_opts = GIT_STATUS_OPTIONS_INIT;
    status_opts.show = GIT_STATUS_SHOW_INDEX_AND_WORKDIR;
    status_opts.flags = GIT_STATUS_OPT_INCLUDE_UNTRACKED |
                        GIT_STATUS_OPT_RECURSE_UNTRACKED_DIRS |
                        GIT_STATUS_OPT_INCLUDE_IGNORED;

    StatusList status_list;
    if (const int error = git_status_list_new(status_list.address(), repo, &status_opts); error != 0) {
      return std::unexpected(std::format("Failed to get repository status: {}", get_git_error()));
    }

    Index repo_index;
    if (const int error = git_repository_index(repo_index.address(), repo); error != 0) {
      return std::unexpected(std::format("Failed to get repository index: {}", get_git_error()));
    }

    const size_t entry_count = git_status_list_entrycount(status_list.get());
    for (const auto i : std::views::iota(0uz, entry_count)) {
      const git_status_entry* entry = git_status_byindex(status_list.get(), i);
      if (entry->head_to_index &&
          (entry->status & (GIT_STATUS_WT_NEW | GIT_STATUS_WT_MODIFIED |
                           GIT_STATUS_WT_DELETED | GIT_STATUS_WT_TYPECHANGE |
                           GIT_STATUS_IGNORED))) {
        const char* path = entry->head_to_index->new_file.path;
        if (const int error = git_index_remove_bypath(repo_index.get(), path); error != 0) {
          std::cout << std::format("Failed to remove file '{}': {}\n", path, get_git_error());
        }
      }
    }

    if (const int error = git_index_write(repo_index.get()); error != 0) {
      return std::unexpected(std::format("Failed to write index: {}", get_git_error()));
    }

    return {};
  }

  [[nodiscard]] GitStringResult
  get_remote_hash_shell(std::string_view upstream) noexcept {
    const std::string ls_remote_cmd = std::format("git ls-remote {}", upstream);
    const std::string ls_remote = exec(ls_remote_cmd.c_str());

    if (const auto tpos = ls_remote.find('\t'); tpos != std::string::npos) {
      return ls_remote.substr(0, tpos);
    }
    return ls_remote;
  }

  [[nodiscard]] GitResult
  clean(git_repository* repo) noexcept {
    Object target;
    if (const int error = git_revparse_single(target.address(), repo, "HEAD"); error != 0) {
      return std::unexpected(std::format("git_revparse_single error: {}", get_git_error()));
    }

    if (const int error = git_reset(repo, target.get(), GIT_RESET_HARD, nullptr); error != 0) {
      return std::unexpected(std::format("git_reset error: {}", get_git_error()));
    }

    return clean_repository(repo);
  }

  [[nodiscard]] std::optional<std::string>
  get_commit_hash(git_reference* head_ref) noexcept {
    const git_oid* commit_head_oid = git_reference_target(head_ref);
    if (!commit_head_oid) return std::nullopt;

    std::string commit_hash(GIT_OID_HEXSZ, '\0');
    git_oid_nfmt(commit_hash.data(), GIT_OID_HEXSZ, commit_head_oid);
    commit_hash.resize(GIT_OID_HEXSZ);

    return commit_hash;
  }

  [[nodiscard]] std::vector<std::string>
  split_upstream(std::string_view upstream) {
    return upstream
      | std::views::split(' ')
      | std::views::transform([](auto&& range) {
          return std::string{range.begin(), range.end()};
        })
      | std::ranges::to<std::vector>();
  }

  [[nodiscard]] GitStringResult
  get_remote_commit_hash(git_remote* remote) noexcept {
    const git_remote_head **refs;
    size_t refs_len;

    if (const int error = git_remote_ls(&refs, &refs_len, remote); error < 0) {
      return std::unexpected(std::format("git_remote_ls error: {}", get_git_error()));
    }

    if (refs_len == 0) {
      return std::unexpected("No remote references found");
    }

    std::string oid(GIT_OID_SHA1_HEXSIZE, '\0');
    git_oid_fmt(oid.data(), &refs[0]->oid);
    return oid;
  }

  [[nodiscard]] GitResult
  do_fetch([[maybe_unused]] git_repository* repo, git_remote* remote) noexcept {
    git_fetch_options fetch_opts = GIT_FETCH_OPTIONS_INIT;
    if (const int error = git_remote_fetch(remote, nullptr, &fetch_opts, nullptr); error != 0) {
      return std::unexpected(std::format("git_remote_fetch error: {}", get_git_error()));
    }
    return {};
  }

  [[nodiscard]] GitResult
  do_checkout(git_repository* repo, git_reference* ref) noexcept {
    git_checkout_options checkout_opts = GIT_CHECKOUT_OPTIONS_INIT;
    checkout_opts.checkout_strategy = GIT_CHECKOUT_FORCE;

    Object target_obj;
    const git_oid* target_oid = git_reference_target(ref);
    if (const int error = git_object_lookup(target_obj.address(), repo, target_oid, GIT_OBJ_ANY); error < 0) {
      return std::unexpected(std::format("git_object_lookup error: {}", get_git_error()));
    }

    if (const int error = git_checkout_tree(repo, target_obj.get(), &checkout_opts); error != 0) {
      return std::unexpected(std::format("git_checkout_tree error: {}", get_git_error()));
    }
    return {};
  }
}

template <> void
Repo <VCS::Git> :: pull (
  const std::shared_ptr<GlobalOptions>& opts
) {
  const GitLibGuard lib_guard;
  const auto& repo_path = target();

  git_repository* raw_repo = nullptr;
  if (const int error = git_repository_open(&raw_repo, repo_path.c_str()); error < 0) {
    std::cout << std::format("libgit2 repository open error: {}\n", get_git_error());
    return;
  }
  GitRepoGuard repo(raw_repo);

  Reference head_ref;
  if (const int error = git_repository_head(head_ref.address(), raw_repo); error < 0) {
    std::cout << std::format("libgit2 repository head error: {}\n", get_git_error());
    return;
  }

  const char* branch_name = nullptr;
  if (const int error = git_branch_name(&branch_name, head_ref.get()); error < 0) {
    std::cout << std::format("libgit2 branch name error: {}\n", get_git_error());
    return;
  }

  const auto& repo_branch = branch();
  if (std::string_view{branch_name} != repo_branch) {
    if (!opts->do_force()) {
      std::cout << std::format( "Not on {}, but on {}, skipping update!\n"
                              , repo_branch, std::string{branch_name} );
      return;
    }

    Reference new_head_ref;
    if (const int error = git_reference_dwim(new_head_ref.address(), raw_repo, repo_branch.c_str()); error < 0) {
      std::cout << std::format("git_reference_dwim error: {}\n", get_git_error());
      return;
    }
    head_ref = std::move(new_head_ref);

    if (const auto result = do_checkout(raw_repo, head_ref.get()); !result) {
      std::cout << std::format("can't checkout to {}: {}\n", repo_branch, result.error());
      return;
    }

    if (opts->is_verbose()) {
      std::cout << std::format("checkout to {} complete\n", repo_branch);
    }

    // Update branch name after successful checkout
    branch_name = repo_branch.data();
  }

  auto local_hash = repo_hash();
  if (local_hash.empty()) {
    if (auto commit_hash = get_commit_hash(head_ref.get())) {
      local_hash = *commit_hash;
      set_hash(local_hash);
    } else {
      std::cout << "Failed to get commit hash\n";
      return;
    }
  }

  const auto upstream_split = split_upstream(upstream());
  if (upstream_split.empty()) {
    std::cout << "Invalid upstream configuration\n";
    return;
  }

  const auto& upstream_remote = upstream_split[0];
  Remote remote;
  if (const int error = git_remote_lookup(remote.address(), raw_repo, upstream_remote.c_str()); error < 0) {
    std::cout << std::format("git_remote_lookup error: {}\n", get_git_error());
    return;
  }

  const int connect_error = git_remote_connect(remote.get(), GIT_DIRECTION_FETCH, nullptr, nullptr, nullptr);

  GitStringResult remote_hash_result;
  bool connected = false;

  if (connect_error < 0) {
    remote_hash_result = get_remote_hash_shell(upstream());
  } else {
    connected = true;
    remote_hash_result = get_remote_commit_hash(remote.get());
  }

  if (!remote_hash_result) {
    std::cout << std::format("Failed to get remote hash: {}\n", remote_hash_result.error());
    return;
  }

  const auto& remote_hash = *remote_hash_result;
  if (local_hash == remote_hash) {
    std::cout << "repository " << *this << " is up to date\n";
    return;
  }

  if (opts->do_clean()) {
    if (auto clean_result = clean(raw_repo); !clean_result) {
      std::cout << std::format("Clean failed: {}\n", clean_result.error());
      return;
    }
  }

  if (connected) {
    if (auto fetch_result = do_fetch(raw_repo, remote.get()); !fetch_result) {
      std::cout << std::format("Fetch failed: {}\n", fetch_result.error());
      return;
    }

    const std::string upstream_branch = upstream_split.size() > 1 ? upstream_split[1] : std::string{branch_name};
    const std::string upstream_ref = std::format("{}/{}", upstream_remote, upstream_branch);

    Reference branch_ref;
    if (const int error = git_branch_lookup(branch_ref.address(), raw_repo, upstream_ref.c_str(), GIT_BRANCH_REMOTE); error != 0) {
      std::cout << std::format("git_branch_lookup error code: {}\n", error);
      return;
    }

    AnnotatedCommit commit;
    if (const int error = git_annotated_commit_from_ref(commit.address(), raw_repo, branch_ref.get()); error != 0) {
      std::cout << std::format("git_annotated_commit_from_ref error: {}\n", get_git_error());
      return;
    }

    const git_oid* commit_oid = git_annotated_commit_id(commit.get());

    Object commit_object;
    if (const int error = git_object_lookup(commit_object.address(), raw_repo, commit_oid, GIT_OBJ_COMMIT); error != 0) {
      std::cout << std::format("git_object_lookup error: {}\n", get_git_error());
      return;
    }

    if (const auto result = do_checkout(raw_repo, branch_ref.get()); !result) {
      std::cout << std::format("Checkout failed: {}\n", result.error());
      return;
    }

    Reference local_branch_ref;
    if (const int error = git_branch_lookup(local_branch_ref.address(), raw_repo, branch_name, GIT_BRANCH_LOCAL); error != 0) {
      std::cout << std::format("git_branch_lookup for local error: {}\n", get_git_error());
      return;
    }

    Reference new_target_ref;
    if (const int error = git_reference_set_target(new_target_ref.address(), local_branch_ref.get(), commit_oid, nullptr); error != 0) {
      std::cout << std::format("git_reference_set_target for local error: {}\n", get_git_error());
      return;
    }

    if (const int error = git_branch_set_upstream(local_branch_ref.get(), upstream_ref.c_str()); error != 0) {
      std::cout << std::format("git_branch_set_upstream error: {}\n", get_git_error());
      return;
    }
  } else {
    const auto pull_cmd = std::format("cd '{}' && git pull {}", target(), upstream());
    const auto output = exec(pull_cmd.c_str());
    if (opts->is_verbose()) {
      std::cout << output << '\n';
    }
  }

  set_hash(remote_hash);
}

template <> void
Repo <VCS::Git> :: rebase (
  const std::shared_ptr<GlobalOptions>& opts
) {
  const GitLibGuard lib_guard;
  const auto& repo_path = target();

  git_repository* raw_repo = nullptr;
  if (const int error = git_repository_open(&raw_repo, repo_path.c_str()); error < 0) {
    std::cout << std::format("libgit2 repository open error: {}\n", get_git_error());
    return;
  }
  GitRepoGuard repo(raw_repo);

  Reference head_ref;
  if (const int error = git_repository_head(head_ref.address(), raw_repo); error < 0) {
    std::cout << std::format("libgit2 repository head error: {}\n", get_git_error());
    return;
  }

  const char* branch_name = nullptr;
  if (const int error = git_branch_name(&branch_name, head_ref.get()); error < 0) {
    std::cout << std::format("libgit2 branch name error: {}\n", get_git_error());
    return;
  }

  const auto& repo_branch = branch();
  if (std::string_view{branch_name} != repo_branch) {
    Reference new_head_ref;
    if (const int error = git_reference_dwim(new_head_ref.address(), raw_repo, repo_branch.c_str()); error < 0) {
      std::cout << std::format("git_reference_dwim error: {}\n", get_git_error());
      return;
    }
    head_ref = std::move(new_head_ref);

    if (const auto result = do_checkout(raw_repo, head_ref.get()); !result) {
      std::cout << std::format("checkout to {} failed: {}\n", repo_branch, result.error());
      return;
    }
    if (opts->is_verbose()) {
      std::cout << std::format("checkout to {} complete\n", repo_branch);
    }

    branch_name = repo_branch.data();
  }

  auto local_hash = repo_hash();
  if (local_hash.empty()) {
    if (auto commit_hash = get_commit_hash(head_ref.get())) {
      local_hash = *commit_hash;
      set_hash(local_hash);
    } else {
      std::cout << "Failed to get commit hash\n";
      return;
    }
  }

  const auto& repo_upstream = upstream();
  const auto remote_hash_result = get_remote_hash_shell(repo_upstream);

  if (!remote_hash_result) {
    std::cout << std::format("Failed to get remote hash: {}\n", remote_hash_result.error());
    return;
  }

  const auto& remote_hash = *remote_hash_result;
  if (local_hash == remote_hash) {
    std::cout << "repository " << *this << " is up to date\n";
    return;
  }

  if (opts->do_clean()) {
    if (auto clean_result = clean(raw_repo); !clean_result) {
      std::cout << std::format("Clean failed: {}\n", clean_result.error());
      return;
    }
  }

  const auto pull_cmd = std::format("cd '{}' && git pull --rebase {}", target(), repo_upstream);
  const auto pull_output = exec(pull_cmd.c_str());
  if (opts->is_verbose()) {
    std::cout << pull_output << '\n';
  }

  const auto push_cmd = std::format("cd '{}' && git push --force origin {}", target(), repo_branch);
  const auto push_output = exec(push_cmd.c_str());
  if (opts->is_verbose()) {
    std::cout << push_output << '\n';
  }

  set_hash(remote_hash);
}
