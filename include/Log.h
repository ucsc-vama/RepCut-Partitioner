//
// Internal logger for librepcut.  Replaces boost::log.
//
// Every logging call goes through rcp_log, which compares the message's
// severity `level` against the caller-provided `ctx_level` (carried in
// RepCutContext::log_level and threaded through every class as a member).
// Output is a single fprintf to stderr — no allocations, no iostreams, no
// global state, safe for concurrent repcut_run() calls on distinct
// contexts.
//
// ERROR messages always print (cannot be suppressed), to make sure no
// silent failure paths exist.
//

#ifndef RCP_LOG_H
#define RCP_LOG_H

#include "repcut.h"

#include <cstdarg>
#include <cstdio>

namespace repcut {

// True if a message at `level` should print given `ctx_level`.
inline bool rcp_should_log(RepCutLogLevel ctx_level, RepCutLogLevel level) {
    if (level == REPCUT_LOG_ERROR) return true;   // errors always print
    return static_cast<int>(level) <= static_cast<int>(ctx_level);
}

// Format-print to stderr if gated.  No-op otherwise.
inline void rcp_log(RepCutLogLevel ctx_level, RepCutLogLevel level, const char* fmt, ...) {
    if (!rcp_should_log(ctx_level, level)) return;
    va_list ap;
    va_start(ap, fmt);
    std::vfprintf(stderr, fmt, ap);
    va_end(ap);
}

}  // namespace repcut

#endif  // RCP_LOG_H