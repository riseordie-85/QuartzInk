#pragma once
#include <cstdint>
namespace trustedtime {
inline int64_t fakeNow = 0;
inline int64_t trustedNow() { return fakeNow; }
}  // namespace trustedtime
