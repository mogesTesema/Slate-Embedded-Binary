// slate/util/intern_table.cc
#include "slate/util/intern_table.hh"

namespace slate {

std::uint32_t InternTable::intern(std::string_view s) {
  auto key = std::string(s);
  if (auto it = index_.find(key); it != index_.end())
    return it->second;

  Entry e;
  e.offset = static_cast<std::uint32_t>(backing_.size());
  e.length = static_cast<std::uint32_t>(s.size());
  backing_.append(s.data(), s.size());

  std::uint32_t id = static_cast<std::uint32_t>(entries_.size());
  entries_.push_back(e);
  index_.emplace(std::move(key), id);
  return id;
}

}  // namespace slate
