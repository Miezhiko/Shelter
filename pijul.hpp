template <> void Repo <VCS::Pijul> :: pull (
  const std::shared_ptr<GlobalOptions>&
) {
  std::cout << "pijul pulls" << std::endl;
}

template <> void Repo <VCS::Pijul> :: rebase (
  const std::shared_ptr<GlobalOptions>&
) {
  std::cout << "pijul rebase!" << std::endl;
}
