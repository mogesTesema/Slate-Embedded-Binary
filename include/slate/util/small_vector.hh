// slate/util/small_vector.hh
//
// A minimal small-buffer-optimized vector. Most cell operand lists, edge sets,
// and evaluation stacks are tiny, so keeping the first N elements inline avoids
// a heap trip on the common path. Only the operations Slate actually needs are
// implemented; this is not a drop-in std::vector.
#ifndef SLATE_UTIL_SMALL_VECTOR_HH
#define SLATE_UTIL_SMALL_VECTOR_HH

#include <cstddef>
#include <cstdlib>
#include <initializer_list>
#include <new>
#include <type_traits>
#include <utility>

namespace slate {

template <typename T, std::size_t N = 8>
class SmallVector {
  static_assert(std::is_trivially_destructible_v<T> ||
                    !std::is_trivially_destructible_v<T>,
                "T must be destructible");

 public:
  SmallVector() : data_(inline_storage()), size_(0), cap_(N) {}

  SmallVector(std::initializer_list<T> init) : SmallVector() {
    reserve(init.size());
    for (const T& v : init) push_back(v);
  }

  SmallVector(const SmallVector& other) : SmallVector() {
    reserve(other.size_);
    for (std::size_t i = 0; i < other.size_; ++i)
      new (data_ + i) T(other.data_[i]);
    size_ = other.size_;
  }

  SmallVector(SmallVector&& other) noexcept : SmallVector() { swap(other); }

  SmallVector& operator=(SmallVector other) {
    swap(other);
    return *this;
  }

  ~SmallVector() {
    clear();
    if (!is_inline()) std::free(data_);
  }

  std::size_t size() const noexcept { return size_; }
  bool empty() const noexcept { return size_ == 0; }
  T* data() noexcept { return data_; }
  const T* data() const noexcept { return data_; }

  T& operator[](std::size_t i) noexcept { return data_[i]; }
  const T& operator[](std::size_t i) const noexcept { return data_[i]; }
  T& back() noexcept { return data_[size_ - 1]; }
  const T& back() const noexcept { return data_[size_ - 1]; }

  T* begin() noexcept { return data_; }
  T* end() noexcept { return data_ + size_; }
  const T* begin() const noexcept { return data_; }
  const T* end() const noexcept { return data_ + size_; }

  void push_back(const T& v) {
    if (size_ == cap_) grow(cap_ * 2);
    new (data_ + size_) T(v);
    ++size_;
  }

  void push_back(T&& v) {
    if (size_ == cap_) grow(cap_ * 2);
    new (data_ + size_) T(std::move(v));
    ++size_;
  }

  template <typename... Args>
  T& emplace_back(Args&&... args) {
    if (size_ == cap_) grow(cap_ * 2);
    new (data_ + size_) T(std::forward<Args>(args)...);
    return data_[size_++];
  }

  void pop_back() noexcept {
    --size_;
    data_[size_].~T();
  }

  void clear() noexcept {
    for (std::size_t i = 0; i < size_; ++i) data_[i].~T();
    size_ = 0;
  }

  void reserve(std::size_t n) {
    if (n > cap_) grow(n);
  }

  void resize(std::size_t n, const T& fill = T()) {
    if (n > cap_) grow(n);
    while (size_ < n) { new (data_ + size_) T(fill); ++size_; }
    while (size_ > n) pop_back();
  }

  void swap(SmallVector& other) noexcept {
    // Both inline: element-wise swap through a scratch buffer path is overkill
    // for our sizes; fall back to a simple move-based exchange.
    if (is_inline() || other.is_inline()) {
      SmallVector tmp;
      tmp.move_from(*this);
      move_from(other);
      other.move_from(tmp);
    } else {
      std::swap(data_, other.data_);
      std::swap(size_, other.size_);
      std::swap(cap_, other.cap_);
    }
  }

 private:
  alignas(T) unsigned char inline_[sizeof(T) * N];

  T* inline_storage() noexcept { return reinterpret_cast<T*>(inline_); }
  bool is_inline() const noexcept {
    return data_ == reinterpret_cast<const T*>(inline_);
  }

  void grow(std::size_t new_cap) {
    if (new_cap < N) new_cap = N;
    auto* fresh = static_cast<T*>(std::malloc(new_cap * sizeof(T)));
    if (!fresh) throw std::bad_alloc();
    for (std::size_t i = 0; i < size_; ++i) {
      new (fresh + i) T(std::move(data_[i]));
      data_[i].~T();
    }
    if (!is_inline()) std::free(data_);
    data_ = fresh;
    cap_ = new_cap;
  }

  void move_from(SmallVector& other) {
    clear();
    reserve(other.size_);
    for (std::size_t i = 0; i < other.size_; ++i)
      new (data_ + i) T(std::move(other.data_[i]));
    size_ = other.size_;
    other.clear();
  }

  T* data_;
  std::size_t size_;
  std::size_t cap_;
};

}  // namespace slate

#endif  // SLATE_UTIL_SMALL_VECTOR_HH
