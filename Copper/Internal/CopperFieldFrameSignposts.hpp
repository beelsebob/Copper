#pragma once

#if defined(__APPLE__)
#include <os/signpost.h>
#else
#include <cstdint>
using os_log_t = void*;
using os_signpost_id_t = std::uint64_t;
inline constexpr os_signpost_id_t OS_SIGNPOST_ID_INVALID = 0;
inline os_log_t os_log_create(const char*, const char*) { return nullptr; }
inline os_signpost_id_t os_signpost_id_generate(os_log_t) { return 0; }
inline void os_signpost_interval_begin(os_log_t, os_signpost_id_t, const char*, ...) {}
inline void os_signpost_interval_end(os_log_t, os_signpost_id_t, const char*, ...) {}
#endif

namespace copper {

inline os_log_t fieldFrameSignpostLog() {
    static os_log_t log = os_log_create("com.tdavie.kiems", "FieldFrames");
    return log;
}

} // namespace copper
