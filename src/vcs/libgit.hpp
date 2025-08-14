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

class GitRepoGuard final {
  public:
  explicit GitRepoGuard(git_repository* repo) noexcept
    : repo_(repo) {}
    
  ~GitRepoGuard() noexcept {
    if (repo_) {
      git_repository_state_cleanup(repo_);
      git_repository_free(repo_);
    }
    git_libgit2_shutdown();
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
  cleanRepository(git_repository* repo) noexcept {
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
  get_remote_hash(std::string_view upstream) noexcept {
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
    
    return cleanRepository(repo);
  }

  [[nodiscard]] std::optional<std::string>
  get_commit_hash(git_reference* head_ref) noexcept {
    const git_oid* commit_head_oid = git_reference_target(head_ref);
    if (!commit_head_oid) return std::nullopt;
    
    git_oid commit_oid;
    git_oid_cpy(&commit_oid, commit_head_oid);
    
    std::string commit_hash(GIT_OID_HEXSZ, '\0');
    git_oid_fmt(commit_hash.data(), &commit_oid);
    
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
}

template <> void
Repo <VCS::Git> :: pull (
  const std::shared_ptr<GlobalOptions>& opts
) {
  const auto& repo_path = target();
  git_repository* repo  = nullptr;
  
  git_libgit2_init();
  GitRepoGuard grg(repo);
  
  if (const int error = git_repository_open(&repo, repo_path.data()); error < 0) {
    std::cout << std::format("libgit2 repository open error: {}\n", get_git_error());
    return;
  }

  grg = GitRepoGuard(repo);

  Reference head_ref;
  if (const int error = git_repository_head(head_ref.address(), repo); error < 0) {
    std::cout << std::format("libgit2 repository head error: {}\n", get_git_error());
    return;
  }

  const char* branch_name = nullptr;
  if (const int error = git_branch_name(&branch_name, head_ref.get()); error < 0) {
    std::cout << std::format("libgit2 branch name error: {}\n", get_git_error());
    return;
  }

  const auto& repo_branch = branch();
  if (branch_name != repo_branch) {
    if (!opts->do_force()) {
      std::cout << std::format( "Not on {}, but on {}, skipping update!\n"
                              , repo_branch, std::string(branch_name) );
      return;
    }
    
    Reference new_head_ref;
    if (const int error = git_reference_dwim(new_head_ref.address(), repo, repo_branch.c_str()); error < 0) {
      std::cout << std::format("git_reference_dwim error: {}\n", get_git_error());
      return;
    }
    head_ref = std::move(new_head_ref);
    
    git_checkout_options gcopts = GIT_CHECKOUT_OPTIONS_INIT;
    const git_oid* target_oid = git_reference_target(head_ref.get());

    Object target_obj;
    if (const int error = git_object_lookup(target_obj.address(), repo, target_oid, GIT_OBJ_ANY); error < 0) {
      std::cout << std::format("can't checkout to {}\n", repo_branch);
      return;
    }

    if (const int error = git_checkout_tree(repo, target_obj.get(), &gcopts); error < 0) {
      std::cout << std::format("git_checkout_tree error: {}\n", get_git_error());
      return;
    }

    if (opts->is_verbose()) {
      std::cout << std::format("checkout to {} complete\n", repo_branch);
    }
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

  Remote remote;
  const auto& upstream_remote = upstream_split[0];
  if (const int error = git_remote_lookup(remote.address(), repo, upstream_remote.c_str()); error < 0) {
    std::cout << std::format("git_remote_lookup error: {}\n", get_git_error());
    return;
  }

  const int connect_error = git_remote_connect(remote.get(), GIT_DIRECTION_FETCH, nullptr, nullptr, nullptr);
  
  GitStringResult remote_hash_result;
  bool connected = false;
  
  if (connect_error < 0) {
    remote_hash_result = get_remote_hash(upstream());
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
    std::cout << "repository " << this << " is up to date\n";
    return;
  }

  if (opts->do_clean()) {
    if (auto clean_result = clean(repo); !clean_result) {
      std::cout << std::format("Clean failed: {}\n", clean_result.error());
      return;
    }
  }

  if (connected) {
    git_fetch_options fetch_opts = GIT_FETCH_OPTIONS_INIT;
    if (const int error = git_remote_fetch(remote.get(), nullptr, &fetch_opts, nullptr); error != 0) {
      std::cout << std::format("git_remote_fetch error: {}\n", get_git_error());
      return;
    }

    const std::string upstream_branch = upstream_split.size() > 1 ? upstream_split[1] : std::string{branch_name};
    const std::string upstream_ref = std::format("{}/{}", upstream_remote, upstream_branch);

    Reference branch_ref;
    if (const int error = git_branch_lookup(branch_ref.address(), repo, upstream_ref.c_str(), GIT_BRANCH_REMOTE); error != 0) {
      std::cout << std::format("git_branch_lookup error code: {}\n", error);
      return;
    }

    AnnotatedCommit commit;
    if (const int error = git_annotated_commit_from_ref(commit.address(), repo, branch_ref.get()); error != 0) {
      std::cout << std::format("git_annotated_commit_from_ref error: {}\n", get_git_error());
      return;
    }

    const git_oid* commit_oid = git_annotated_commit_id(commit.get());

    Object commit_object;
    if (const int error = git_object_lookup(commit_object.address(), repo, commit_oid, GIT_OBJECT_COMMIT); error != 0) {
      std::cout << std::format("git_object_lookup error: {}\n", get_git_error());
      return;
    }

    git_checkout_options checkout_opts = GIT_CHECKOUT_OPTIONS_INIT;
    checkout_opts.checkout_strategy = GIT_CHECKOUT_FORCE;
    if (const int error = git_checkout_tree(repo, commit_object.get(), &checkout_opts); error != 0) {
      std::cout << std::format("git_checkout_tree error: {}\n", get_git_error());
      return;
    }

    Reference local_branch_ref;
    if (const int error = git_branch_lookup(local_branch_ref.address(), repo, branch_name, GIT_BRANCH_LOCAL); error != 0) {
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
    const auto pull_cmd = std::format("git pull {}", upstream());
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
  git_libgit2_init();

  const auto& repo_branch = branch();
  const auto& repo_path   = target();

  git_repository* repo = nullptr;
  GitRepoGuard grg(repo);

  if (const int error = git_repository_open(&repo, repo_path.data()); error < 0) {
    std::cout << std::format("libgit2 repository open error: {}\n", get_git_error());
    return;
  }

  grg = GitRepoGuard(repo);

  Reference head_ref;
  if (const int error = git_repository_head(head_ref.address(), repo); error < 0) {
    std::cout << std::format("libgit2 repository head error: {}\n", get_git_error());
    return;
  }

  const char* branch_name = nullptr;
  if (const int error = git_branch_name(&branch_name, head_ref.get()); error < 0) {
    std::cout << std::format("libgit2 branch name error: {}\n", get_git_error());
    return;
  }

  if (branch_name != repo_branch) {
    Reference new_head_ref;
    if (const int error = git_reference_dwim(new_head_ref.address(), repo, repo_branch.c_str()); error < 0) {
      std::cout << std::format("git_reference_dwim error: {}\n", get_git_error());
      return;
    }
    head_ref = std::move(new_head_ref);
    
    git_checkout_options gcopts = GIT_CHECKOUT_OPTIONS_INIT;
    if (const int error = git_checkout_tree(repo, reinterpret_cast<const git_object*>(git_reference_target(head_ref.get())), &gcopts); error < 0) {
      std::cout << std::format("git_checkout_tree error: {}\n", get_git_error());
      return;
    }
    if (opts->is_verbose()) {
      std::cout << std::format("checkout to {} complete\n", repo_branch);
    }
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
  const auto remote_hash_result = get_remote_hash(repo_upstream);
  
  if (!remote_hash_result) {
    std::cout << std::format("Failed to get remote hash: {}\n", remote_hash_result.error());
    return;
  }

  const auto& remote_hash = *remote_hash_result;
  if (local_hash == remote_hash) {
    std::cout << "repository " << this << " is up to date\n";
    return;
  }

  if (opts->do_clean()) {
    if (auto clean_result = clean(repo); !clean_result) {
      std::cout << std::format("Clean failed: {}\n", clean_result.error());
      return;
    }
  }

  const auto pull_cmd = std::format("git pull --rebase {}", repo_upstream);
  const auto pull_output = exec(pull_cmd.c_str());
  if (opts->is_verbose()) {
    std::cout << pull_output << '\n';
  }

  const auto push_cmd = std::format("git push --force origin {}", repo_branch);
  const auto push_output = exec(push_cmd.c_str());
  if (opts->is_verbose()) {
    std::cout << push_output << '\n';
  }

  set_hash(remote_hash);
}
