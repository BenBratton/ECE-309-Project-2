// include/core/sentinel_scanner.h
// YOURS — incrementally scans a chunked stream for a stop sentinel,
// holding back at most sentinel_.size() - 1 characters at any time.

#pragma once
#include <string>
#include <string_view>

class SentinelScanner {
public:
    // Throws std::invalid_argument if sentinel is empty.
    explicit SentinelScanner(std::string sentinel);

    struct Out { std::string safe_text; bool sentinel_found; };

    // Feed the next chunk. Returns text guaranteed NOT to be part of
    // the sentinel (safe to print immediately) and whether the
    // sentinel has now been fully seen. The sentinel itself is never
    // included in safe_text. Once found, later chunks are discarded.
    Out feed(std::string_view chunk);

    // Call once, after the stream ends, to release any text still
    // being held back.
    Out flush();

private:
    std::string sentinel_;
    std::string pending_;   // holds back at most sentinel_.size() - 1
                            // trailing characters that could still
                            // become the start of the sentinel
    bool found_ = false;    // sentinel already seen; ignore further input
};