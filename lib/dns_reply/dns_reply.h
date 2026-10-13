// Builds the reply of a captive-portal DNS server: every name resolves to the
// access point's own IPv4 address. Pure C++, tested on the host.
#pragma once
#include <cstddef>
#include <cstdint>

namespace dns_reply {

constexpr size_t kMaxPacket = 512;  // plain UDP DNS

// Answer the query in `q` (`q_len` bytes) into `out`. A/IN questions get one A
// record pointing at `ipv4`; any other type gets NOERROR with no answer, so the
// client falls back to IPv4. EDNS and other additional records in the query are
// ignored and never echoed. Returns the reply length, or 0 when `q` is not a
// well-formed single-question standard query or `out_cap` is too small.
size_t build(const uint8_t* q, size_t q_len, const uint8_t ipv4[4], uint8_t* out, size_t out_cap);

}  // namespace dns_reply
