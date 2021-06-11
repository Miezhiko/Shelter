template <> void Repo <VCS::Pijul> :: pull (
  const std::shared_ptr<GlobalOptions>&
) const {
  std::cout << "pijul pulls" << std::endl;
}

template <> void Repo <VCS::Pijul> :: rebase (
  const std::shared_ptr<GlobalOptions>&
) const {
  std::cout << "pijul rebase!" << std::endl;
}
