#include <unity.h>
#include <vector>

#include "pulse_log.h"
#include "speed_types.h"

using namespace speed_core;

static PulseRecord rec(uint32_t seq) {
  PulseRecord r{};
  r.seq = seq;
  r.t_us = seq * 1000u;
  return r;
}

void setUp() {}
void tearDown() {}

// --- PulseLog ---

void test_log_starts_empty() {
  PulseLog log;
  TEST_ASSERT_EQUAL_UINT32(0, log.size());
  TEST_ASSERT_EQUAL_UINT32(0, log.lastSeq());
  TEST_ASSERT_EQUAL_UINT32(0, log.overwritten());
  TEST_ASSERT_EQUAL_UINT32(LOG_CAPACITY, log.capacity());
}

void test_log_push_and_at_oldest_first() {
  PulseLog log;
  log.push(rec(1));
  log.push(rec(2));
  log.push(rec(3));
  TEST_ASSERT_EQUAL_UINT32(3, log.size());
  TEST_ASSERT_EQUAL_UINT32(1, log.at(0).seq);
  TEST_ASSERT_EQUAL_UINT32(3, log.at(2).seq);
  TEST_ASSERT_EQUAL_UINT32(3, log.lastSeq());
}

void test_log_wraps_and_counts_overwritten() {
  PulseLog log;
  for (uint32_t i = 1; i <= LOG_CAPACITY + 10; ++i) log.push(rec(i));
  TEST_ASSERT_EQUAL_UINT32(LOG_CAPACITY, log.size());
  TEST_ASSERT_EQUAL_UINT32(10, log.overwritten());
  TEST_ASSERT_EQUAL_UINT32(11, log.at(0).seq);                  // oldest surviving
  TEST_ASSERT_EQUAL_UINT32(LOG_CAPACITY + 10, log.lastSeq());
}

void test_log_since_returns_tail_in_order_capped() {
  PulseLog log;
  for (uint32_t i = 1; i <= LOG_CAPACITY + 10; ++i) log.push(rec(i));
  std::vector<uint32_t> seen;
  size_t n = log.since(LOG_CAPACITY + 5, 100, [&](const PulseRecord& r) { seen.push_back(r.seq); });
  TEST_ASSERT_EQUAL_UINT32(5, n);
  TEST_ASSERT_EQUAL_UINT32(LOG_CAPACITY + 6, seen.front());
  TEST_ASSERT_EQUAL_UINT32(LOG_CAPACITY + 10, seen.back());

  seen.clear();
  n = log.since(0, 3, [&](const PulseRecord& r) { seen.push_back(r.seq); });
  TEST_ASSERT_EQUAL_UINT32(3, n);
  TEST_ASSERT_EQUAL_UINT32(11, seen.front());  // cap applies from the oldest
}

void test_log_since_older_than_retained_returns_everything() {
  PulseLog log;
  for (uint32_t i = 1; i <= LOG_CAPACITY + 10; ++i) log.push(rec(i));
  size_t n = log.since(3, 100000, [](const PulseRecord&) {});
  TEST_ASSERT_EQUAL_UINT32(LOG_CAPACITY, n);
}

void test_log_clear() {
  PulseLog log;
  log.push(rec(1));
  log.clear();
  TEST_ASSERT_EQUAL_UINT32(0, log.size());
  TEST_ASSERT_EQUAL_UINT32(0, log.lastSeq());
  TEST_ASSERT_EQUAL_UINT32(0, log.overwritten());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_log_starts_empty);
  RUN_TEST(test_log_push_and_at_oldest_first);
  RUN_TEST(test_log_wraps_and_counts_overwritten);
  RUN_TEST(test_log_since_returns_tail_in_order_capped);
  RUN_TEST(test_log_since_older_than_retained_returns_everything);
  RUN_TEST(test_log_clear);
  return UNITY_END();
}
