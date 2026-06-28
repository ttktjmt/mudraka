#include <doctest/doctest.h>

#include <array>
#include <vector>

#include "mudraka/ring_buffer.hpp"

using mudraka::RingBuffer;

namespace {
// Helper: read into 3 channel vectors via the int32_t* const* interface.
struct Dst {
  std::array<std::vector<int32_t>, 3> ch;
  std::array<int32_t*, 3> ptr;
  explicit Dst(uint64_t n) {
    for (int c = 0; c < 3; ++c) {
      ch[c].assign(n, 0);
      ptr[c] = ch[c].data();
    }
  }
  int32_t* const* p() { return ptr.data(); }
};
}  // namespace

TEST_CASE("RingBuffer basic push/read") {
  RingBuffer rb(3, 10);
  for (int i = 0; i < 5; ++i) {
    int32_t s[3] = {i, i * 2, i * 3};
    rb.push(s);
  }
  CHECK(rb.total_written() == 5);

  Dst d(10);
  auto r = rb.read(0, d.p(), 10);
  CHECK(r.written == 5);
  CHECK(r.lost == 0);
  CHECK(r.next_cursor == 5);
  for (int i = 0; i < 5; ++i) {
    CHECK(d.ch[0][i] == i);
    CHECK(d.ch[1][i] == i * 2);
    CHECK(d.ch[2][i] == i * 3);
  }
}

TEST_CASE("RingBuffer overwrite-oldest reports loss") {
  RingBuffer rb(3, 10);
  for (int i = 0; i < 13; ++i) {  // 3 oldest overwritten
    int32_t s[3] = {i, i, i};
    rb.push(s);
  }
  CHECK(rb.total_written() == 13);

  Dst d(20);
  auto r = rb.read(0, d.p(), 20);
  CHECK(r.lost == 3);             // samples 0,1,2 gone
  CHECK(r.written == 10);         // 3..12
  CHECK(r.next_cursor == 13);
  CHECK(d.ch[0][0] == 3);
  CHECK(d.ch[0][9] == 12);
}

TEST_CASE("RingBuffer cursor read advances without loss when consumer keeps up") {
  RingBuffer rb(3, 100);
  Dst d(100);
  uint64_t cursor = 0;
  for (int round = 0; round < 3; ++round) {
    for (int i = 0; i < 20; ++i) {
      int32_t s[3] = {round * 100 + i, 0, 0};
      rb.push(s);
    }
    auto r = rb.read(cursor, d.p(), 100);
    CHECK(r.lost == 0);
    CHECK(r.written == 20);
    cursor = r.next_cursor;
  }
  CHECK(cursor == 60);
}

TEST_CASE("RingBuffer envelope min/max") {
  RingBuffer rb(1, 100);
  for (int i = 0; i < 100; ++i) {
    int32_t s[1] = {i % 10 - 5};  // ranges -5..4 repeatedly
    rb.push(s);
  }
  std::vector<int32_t> mn(4), mx(4);
  int32_t* mnp[1] = {mn.data()};
  int32_t* mxp[1] = {mx.data()};
  auto nb = rb.envelope(0, 100, 4, mnp, mxp);
  CHECK(nb == 4);
  for (uint32_t b = 0; b < nb; ++b) {
    CHECK(mn[b] <= mx[b]);
    CHECK(mn[b] >= -5);
    CHECK(mx[b] <= 4);
  }
}
