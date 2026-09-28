// src/sentinel_scanner.cpp

#include "core/sentinel_scanner.h"
#include <algorithm>
#include <stdexcept>
#include <utility>

SentinelScanner::SentinelScanner(std::string sentinel)
    : sentinel_(std::move(sentinel)) {
    if (sentinel_.empty()) {
        throw std::invalid_argument("SentinelScanner: sentinel must not be empty");
    }
}

SentinelScanner::Out SentinelScanner::feed(std::string_view chunk) {
    if (found_) {
        return {"", true};  // reply is over; discard anything after the sentinel
    }

    // Only pending_ (< sentinel_.size() chars) plus this chunk is ever searched.
    std::string buf = std::move(pending_);
    buf.append(chunk);

    // Case 1: the full sentinel is in the buffer.
    std::size_t pos = buf.find(sentinel_);
    if (pos != std::string::npos) {
        found_ = true;
        pending_.clear();
        buf.resize(pos);            // keep only the text before the sentinel
        return {std::move(buf), true};
    }

    // Case 2: no full match. Hold back the longest suffix of buf that is a
    // proper prefix of the sentinel; everything before it is safe to emit.
    std::size_t max_hold = std::min(buf.size(), sentinel_.size() - 1);
    std::size_t hold = 0;
    for (std::size_t k = max_hold; k > 0; --k) {
        if (buf.compare(buf.size() - k, k, sentinel_, 0, k) == 0) {
            hold = k;
            break;
        }
    }

    pending_.assign(buf, buf.size() - hold, hold);
    buf.resize(buf.size() - hold);
    return {std::move(buf), false};
}

SentinelScanner::Out SentinelScanner::flush() {
    Out out{std::move(pending_), found_};
    pending_.clear();
    return out;
}