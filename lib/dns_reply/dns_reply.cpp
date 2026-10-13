#include "dns_reply.h"

#include <cstring>

namespace dns_reply {

namespace {
constexpr size_t kHeader = 12;
constexpr uint16_t kTypeA = 1;
constexpr uint16_t kClassIn = 1;
constexpr uint32_t kTtlS = 60;

uint16_t be16(const uint8_t* p) { return static_cast<uint16_t>((p[0] << 8) | p[1]); }
}  // namespace

size_t build(const uint8_t* q, size_t q_len, const uint8_t ipv4[4], uint8_t* out, size_t out_cap) {
  if (q_len < kHeader) return 0;
  const bool is_response = q[2] & 0x80;
  const uint8_t opcode = (q[2] >> 3) & 0x0F;
  if (is_response || opcode != 0 || be16(q + 4) != 1) return 0;

  // Question name: uncompressed labels ending in a zero byte, at most 255 bytes.
  size_t pos = kHeader;
  while (true) {
    if (pos >= q_len) return 0;
    const uint8_t len = q[pos];
    if (len == 0) break;
    if (len & 0xC0) return 0;  // no compression pointers in a query
    pos += 1 + len;
    if (pos - kHeader > 255) return 0;
  }
  const size_t question_end = pos + 1 + 4;  // zero byte, QTYPE, QCLASS
  if (question_end > q_len) return 0;
  const uint16_t qtype = be16(q + pos + 1);
  const uint16_t qclass = be16(q + pos + 3);
  const bool answer = qtype == kTypeA && qclass == kClassIn;

  const size_t question_len = question_end - kHeader;
  const size_t reply_len = kHeader + question_len + (answer ? 16 : 0);
  if (reply_len > out_cap) return 0;

  out[0] = q[0];  // ID
  out[1] = q[1];
  out[2] = static_cast<uint8_t>(0x80 | 0x04 | (q[2] & 0x01));  // QR, AA, copy RD
  out[3] = 0x00;                                               // RA 0, RCODE NOERROR
  out[4] = 0; out[5] = 1;                                      // QDCOUNT
  out[6] = 0; out[7] = answer ? 1 : 0;                         // ANCOUNT
  out[8] = out[9] = out[10] = out[11] = 0;                     // NSCOUNT, ARCOUNT
  std::memcpy(out + kHeader, q + kHeader, question_len);

  if (answer) {
    uint8_t* a = out + kHeader + question_len;
    a[0] = 0xC0; a[1] = kHeader;                  // name: pointer to the question
    a[2] = 0; a[3] = kTypeA;
    a[4] = 0; a[5] = kClassIn;
    a[6] = 0; a[7] = 0; a[8] = 0; a[9] = kTtlS;
    a[10] = 0; a[11] = 4;
    std::memcpy(a + 12, ipv4, 4);
  }
  return reply_len;
}

}  // namespace dns_reply
