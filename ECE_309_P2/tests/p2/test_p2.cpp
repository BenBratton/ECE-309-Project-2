// tests/p2/test_p2.cpp
//
// YOUR test suite goes here. At least 12 assert-based test cases — see
// spec §5 for the required categories and the sample test for the
// expected level of rigor.
//
// This file is a stub so the project builds out of the box; replace the
// body of main() with your own tests.

//   1 Empty bounds        -> EmptyConversationBounds, AtIsBoundsChecked
//   2 System ordering     -> SystemMessageStaysPinned, HarnessSystemMessagePinned
//   3 Rule of Five (copy) -> CopyConstructorIsDeep, CopyAssignmentIsDeep
//   4 Rule of Five (move) -> MoveConstructorStealsAndZeroes, MoveAssignmentStealsAndZeroes
//   5 Growth behavior     -> GrowthDoublesAndPreservesContents
//   6 Scanner clean text  -> ScannerCleanText, ScannerChunkSizeIndependence
//   7 Scanner splits      -> ScannerSplitAtEveryBoundary, ScannerThreeWaySplits,
//                            ScannerOneCharAtATime
//   8 Scanner false alarm -> ScannerFalseAlarms, ScannerSelfOverlappingSentinel
//   9 Scanner bounded mem -> ScannerBoundedMemory4MB
//  10 Harness turn limit  -> HarnessTurnLimit
//  11 Harness sentinel    -> HarnessSentinelHalt
//  12 Transcript trip     -> TranscriptRoundTrip
//  (+) EOF                -> HarnessEofExit

#ifdef NDEBUG
#undef NDEBUG
#endif

#include "core/conversation.h"
#include "core/message.h"
#include "core/sentinel_scanner.h"
#include "harness/harness.h"
#include "model/replay_client.h"
#include "model/scripted_client.h"

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>



#define TEST(name) static void name()
#define RUN(name)                                  \
    do {                                           \
        name();                                    \
        std::cout << "[PASS] " << #name << "\n";   \
    } while (0)

namespace {

const std::string kSentinel = "<|end_conversation|>";

Message msg(Role r, const std::string& text) { return Message(r, text); }

bool at_throws(const Conversation& c, std::size_t i) {
    try {
        (void)c.at(i);
    } catch (const std::out_of_range&) {
        return true;
    }
    return false;
}

// Long enough to defeat the small-string optimisation, so "different string
// buffer" is a meaningful check.
std::string long_text(int i) {
    return "message number " + std::to_string(i) +
           " padded out so this string lives on the heap, not in the SSO buffer";
}

void fill(Conversation& c, int n) {
    for (int i = 0; i < n; ++i) {
        c.append(msg(i % 2 ? Role::Assistant : Role::User, long_text(i)));
    }
}

bool same_contents(const Conversation& a, const Conversation& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a.at(i).role() != b.at(i).role()) return false;
        if (a.at(i).content() != b.at(i).content()) return false;
    }
    return true;
}

// ---- scanner helpers -------------------------------------------------------

struct ScanResult {
    std::string text;
    bool found = false;
    std::size_t found_on_call = 0;  // 1-based index of the feed() that reported it
    std::size_t calls = 0;
};

void feed_into(SentinelScanner& sc, std::string_view chunk, ScanResult& r) {
    auto out = sc.feed(chunk);
    ++r.calls;
    r.text += out.safe_text;
    if (out.sentinel_found && !r.found) {
        r.found = true;
        r.found_on_call = r.calls;
    }
}

// ---- harness helpers -------------------------------------------------------

class LineInput : public InputSource {
public:
    explicit LineInput(const std::string& lines) : in_(lines) {}
    std::string read_line() override {
        std::string line;
        if (!std::getline(in_, line)) {
            eof_ = true;
            return "";
        }
        ++lines_read_;
        return line;
    }
    bool is_eof() const override { return eof_; }
    int lines_read() const { return lines_read_; }

private:
    std::istringstream in_;
    bool eof_ = false;
    int lines_read_ = 0;
};

class CollectOutput : public OutputSink {
public:
    void write(std::string_view text) override { text_.append(text); }
    const std::string& text() const { return text_; }

private:
    std::string text_;
};

// Writes a file for the duration of a test, removes it afterwards.
class TempFile {
public:
    TempFile(const std::string& name, const std::string& content)
        : path_((std::filesystem::temp_directory_path() / name).string()) {
        std::ofstream f(path_);
        f << content;
    }
    ~TempFile() { std::remove(path_.c_str()); }
    TempFile(const TempFile&) = delete;
    TempFile& operator=(const TempFile&) = delete;
    const std::string& path() const { return path_; }

private:
    std::string path_;
};

Harness make_scripted_harness(const std::string& script_path, int max_turns) {
    auto client = std::make_unique<ScriptedModelClient>(script_path);
    HarnessConfig cfg;
    cfg.max_turns = max_turns;
    cfg.system_message = client->system_message();
    return Harness(std::move(client), cfg);
}

const char* role_name(Role r) {
    switch (r) {
        case Role::System: return "system";
        case Role::User: return "user";
        case Role::Assistant: return "assistant";
    }
    return "assistant";
}

// Same format as the provided main.cpp's save_transcript (Appendix A).
void save_transcript(const Conversation& conv, const std::string& path) {
    std::ofstream file(path);
    bool first = true;
    for (const Message* m = conv.begin(); m != conv.end(); ++m) {
        if (!first) file << "---\n";
        first = false;
        file << "role: " << role_name(m->role()) << "\n";
        file << m->content() << "\n";
    }
}

}  // namespace

// ============================================================================
// Conversation
// ============================================================================

// Spec 1
TEST(EmptyConversationBounds) {
    Conversation c;
    assert(c.size() == 0);
    assert(c.capacity() == 0);
    assert(c.begin() == c.end());

    int visited = 0;
    for (const Message& m : c) {
        (void)m;
        ++visited;
    }
    assert(visited == 0);
    assert(at_throws(c, 0));

    Conversation copy(c);
    assert(copy.size() == 0 && copy.begin() == copy.end());
    Conversation moved(std::move(c));
    assert(moved.size() == 0 && moved.begin() == moved.end());
    Conversation assigned;
    assigned = copy;
    assert(assigned.size() == 0);
}

// Spec 1: the at() behaviour I chose (throw std::out_of_range) is tested.
TEST(AtIsBoundsChecked) {
    Conversation c;
    fill(c, 3);
    assert(!at_throws(c, 0));
    assert(!at_throws(c, 2));
    assert(at_throws(c, 3));  // exactly size()
    assert(at_throws(c, 4));
    assert(at_throws(c, static_cast<std::size_t>(-1)));
    assert(c.at(2).content() == long_text(2));
}

// Spec 2 (container level)
TEST(SystemMessageStaysPinned) {
    Conversation c;
    c.append(msg(Role::System, "Be concise."));
    for (int i = 0; i < 200; ++i) {  // forces several reallocations
        c.append(msg(i % 2 ? Role::Assistant : Role::User, std::to_string(i)));
        assert(c.at(0).role() == Role::System);
        assert(c.at(0).content() == "Be concise.");
    }
    assert(c.size() == 201);
    for (std::size_t i = 1; i < c.size(); ++i) {
        assert(c.at(i).role() != Role::System);
        assert(c.at(i).content() == std::to_string(i - 1));  // order preserved
    }
}

// Spec 3
TEST(CopyConstructorIsDeep) {
    Conversation original;
    fill(original, 7);

    Conversation copy(original);
    assert(copy.begin() != original.begin());  // separate array
    assert(&copy.at(0) != &original.at(0));
    assert(same_contents(copy, original));
    // Strings were copied too, not shared.
    assert(copy.at(0).content().data() != original.at(0).content().data());

    // Independent afterwards: growing one does not touch the other.
    original.append(msg(Role::User, "extra"));
    assert(original.size() == 8 && copy.size() == 7);

    // The copy survives the original's destruction (a shallow copy would be a
    // double free / use-after-free that AddressSanitizer reports).
    Conversation survivor(original);
    {
        Conversation temp;
        fill(temp, 5);
        Conversation temp_copy(temp);
        survivor = temp_copy;
    }
    assert(survivor.size() == 5);
    assert(survivor.at(4).content() == long_text(4));
}

// Spec 3
TEST(CopyAssignmentIsDeep) {
    Conversation src;
    fill(src, 10);
    Conversation dst;
    dst.append(msg(Role::User, "old contents"));
    dst.append(msg(Role::User, "that must be replaced"));

    dst = src;
    assert(dst.begin() != src.begin());
    assert(same_contents(dst, src));
    assert(dst.at(0).content().data() != src.at(0).content().data());

    // Self-assignment leaves the object intact.
    Conversation& alias = dst;
    dst = alias;
    assert(dst.size() == 10 && dst.at(9).content() == long_text(9));

    // Assigning an empty conversation empties the target.
    Conversation empty;
    dst = empty;
    assert(dst.size() == 0 && dst.begin() == dst.end());

    // The source was never disturbed.
    assert(src.size() == 10 && src.at(9).content() == long_text(9));
}

// Spec 4
TEST(MoveConstructorStealsAndZeroes) {
    Conversation a;
    fill(a, 5);
    const Message* buffer = a.begin();
    const std::size_t cap = a.capacity();

    Conversation b(std::move(a));
    assert(b.begin() == buffer);  // same buffer: stolen, not copied
    assert(b.size() == 5 && b.capacity() == cap);
    assert(b.at(4).content() == long_text(4));

    assert(a.size() == 0 && a.capacity() == 0);  // source zeroed
    assert(a.begin() == a.end());
    assert(at_throws(a, 0));

    // Moved-from object is valid: reusable, and independent of b.
    a.append(msg(Role::User, "reborn"));
    assert(a.size() == 1 && a.at(0).content() == "reborn");
    assert(b.begin() == buffer && b.size() == 5);
}

// Spec 4
TEST(MoveAssignmentStealsAndZeroes) {
    Conversation src;
    fill(src, 6);
    const Message* buffer = src.begin();

    Conversation dst;
    fill(dst, 3);  // its old buffer must be released (ASan checks for the leak)

    dst = std::move(src);
    assert(dst.begin() == buffer && dst.size() == 6);
    assert(dst.at(5).content() == long_text(5));
    assert(src.size() == 0 && src.capacity() == 0 && src.begin() == src.end());

    src.append(msg(Role::User, "still usable"));
    assert(src.size() == 1);

    // Self-move must not destroy the data.
    Conversation& alias = dst;
    dst = std::move(alias);
    assert(dst.size() == 6 && dst.at(0).content() == long_text(0));
}

// Spec 5
TEST(GrowthDoublesAndPreservesContents) {
    Conversation c;
    assert(c.capacity() == 0);  // no allocation until the first append

    std::size_t reallocations = 0;
    std::size_t prev_cap = 0;
    for (int i = 0; i < 1000; ++i) {
        c.append(msg(Role::User, std::to_string(i)));
        assert(c.size() == static_cast<std::size_t>(i) + 1);
        assert(c.capacity() >= c.size());

        if (c.capacity() != prev_cap) {
            ++reallocations;
            // Growth factor 2 (first allocation is 4).
            assert(c.capacity() == (prev_cap == 0 ? 4 : prev_cap * 2));
            prev_cap = c.capacity();
            // Everything survived the reallocation, in order.
            for (int j = 0; j <= i; ++j) {
                assert(c.at(static_cast<std::size_t>(j)).content() == std::to_string(j));
            }
        }
    }
    // 4, 8, 16, ..., 1024 -> 9 allocations for 1000 appends: O(log n), not O(n).
    assert(reallocations == 9);
    assert(c.capacity() == 1024);
    for (int i = 0; i < 1000; ++i) {
        assert(c.at(static_cast<std::size_t>(i)).content() == std::to_string(i));
    }
}

// ============================================================================
// SentinelScanner
// ============================================================================

// Spec 6
TEST(ScannerCleanText) {
    SentinelScanner sc(kSentinel);
    auto a = sc.feed("Hello, world! No stop token here.");
    assert(!a.sentinel_found);
    // Nothing in this text could start the sentinel, so nothing is held back.
    assert(a.safe_text == "Hello, world! No stop token here.");
    auto b = sc.feed("");
    assert(b.safe_text.empty() && !b.sentinel_found);
    auto f = sc.flush();
    assert(f.safe_text.empty() && !f.sentinel_found);

    // Whole sentinel in one chunk: text before it is emitted, sentinel is not.
    SentinelScanner sc2(kSentinel);
    auto w = sc2.feed("Goodbye." + kSentinel);
    assert(w.sentinel_found && w.safe_text == "Goodbye.");

    // Anything after the sentinel is discarded, and later calls stay "done".
    SentinelScanner sc3(kSentinel);
    auto x = sc3.feed("Hi" + kSentinel + "trailing junk");
    assert(x.sentinel_found && x.safe_text == "Hi");
    auto y = sc3.feed("more junk");
    assert(y.sentinel_found && y.safe_text.empty());
    auto z = sc3.flush();
    assert(z.safe_text.empty());
}

// Spec 6: the result must not depend on where the chunk boundaries fall.
TEST(ScannerChunkSizeIndependence) {
    const std::string text = "The quick brown fox" + kSentinel + "jumps over";
    for (std::size_t chunk = 1; chunk <= 45; ++chunk) {
        SentinelScanner sc(kSentinel);
        ScanResult r;
        for (std::size_t i = 0; i < text.size(); i += chunk) {
            feed_into(sc, std::string_view(text).substr(i, chunk), r);
        }
        assert(r.found);
        assert(r.text == "The quick brown fox");
    }
}

// Spec 7: the spec's sample test.
TEST(ScannerSplitAtEveryBoundary) {
    const std::string text = "Goodbye." + kSentinel;
    for (std::size_t split = 0; split <= text.size(); ++split) {
        SentinelScanner scanner(kSentinel);
        auto out1 = scanner.feed(text.substr(0, split));
        auto out2 = scanner.feed(text.substr(split));
        assert((out1.sentinel_found || out2.sentinel_found) &&
               "sentinel must be caught regardless of split point");
        assert(out1.safe_text + out2.safe_text == "Goodbye.");
        // The first call must never leak part of the sentinel, and must not
        // claim a match before the whole sentinel has arrived.
        assert(std::string("Goodbye.").rfind(out1.safe_text, 0) == 0);
        if (split < text.size()) assert(!out1.sentinel_found);
    }
}

// Spec 7: two split points -> three chunks.
TEST(ScannerThreeWaySplits) {
    const std::string text = "ab" + kSentinel;
    for (std::size_t i = 0; i <= text.size(); ++i) {
        for (std::size_t j = i; j <= text.size(); ++j) {
            SentinelScanner sc(kSentinel);
            ScanResult r;
            feed_into(sc, std::string_view(text).substr(0, i), r);
            feed_into(sc, std::string_view(text).substr(i, j - i), r);
            feed_into(sc, std::string_view(text).substr(j), r);
            assert(r.found);
            assert(r.text == "ab");
        }
    }
}

// Spec 7: the extreme case the autograder stress-tests.
TEST(ScannerOneCharAtATime) {
    const std::string text = "Goodbye." + kSentinel;
    SentinelScanner sc(kSentinel);
    ScanResult r;
    for (char c : text) {
        feed_into(sc, std::string_view(&c, 1), r);
    }
    assert(r.found);
    assert(r.found_on_call == text.size());  // only on the very last character
    assert(r.text == "Goodbye.");
    auto f = sc.flush();
    assert(f.safe_text.empty());
}

// Spec 8
TEST(ScannerFalseAlarms) {
    // Near-misses must come out unchanged and never trigger a match.
    const char* near_misses[] = {
        "<|end_world|>",
        "<|end_conversation|",    // missing the final '>'
        "<|end_conversation|x>",
        "<|end_conversation>",
        "|end_conversation|>",    // missing the leading '<'
        "<|END_CONVERSATION|>",   // case matters
        "<|end_ conversation|>",
        "<|end_",
        "<",
    };
    for (const char* s : near_misses) {
        SentinelScanner sc(kSentinel);
        ScanResult r;
        for (const char* p = s; *p; ++p) feed_into(sc, std::string_view(p, 1), r);
        auto f = sc.flush();
        r.text += f.safe_text;
        assert(!r.found);
        assert(r.text == s);  // nothing lost, nothing invented
    }

    // A false start immediately before the real sentinel must not hide it.
    struct Case { const char* in; const char* expected; };
    const Case cases[] = {
        {"<|<|end_conversation|>", "<|"},
        {"<<|end_conversation|>", "<"},
        {"a<|end_b<|end_conversation|>", "a<|end_b"},
        {"<|end_conversation|<|end_conversation|>", "<|end_conversation|"},
    };
    for (const Case& c : cases) {
        SentinelScanner one(kSentinel);
        ScanResult r;
        for (const char* p = c.in; *p; ++p) feed_into(one, std::string_view(p, 1), r);
        assert(r.found && r.text == c.expected);

        SentinelScanner whole(kSentinel);
        auto out = whole.feed(c.in);
        assert(out.sentinel_found && out.safe_text == c.expected);
    }
}

// Spec 8: a sentinel that overlaps itself is where naive "reset on
// mismatch" scanners fail (e.g. "aaab" for the sentinel "aab").
TEST(ScannerSelfOverlappingSentinel) {
    const char* inputs[] = {"aaab", "aaaab", "xaabaab"};
    const char* expected[] = {"a", "aa", "x"};
    for (int k = 0; k < 3; ++k) {
        SentinelScanner sc("aab");
        ScanResult r;
        for (const char* p = inputs[k]; *p; ++p) feed_into(sc, std::string_view(p, 1), r);
        assert(r.found);
        assert(r.text == expected[k]);
    }

    SentinelScanner ab("abab");
    ScanResult r;
    for (const char* p = "abaabab"; *p; ++p) feed_into(ab, std::string_view(p, 1), r);
    assert(r.found && r.text == "aba");
}

// Spec 9: pending_ is private, so measure it from the outside: every byte fed
// that has not yet been emitted IS the pending buffer. Assert that number never
// exceeds sentinel.size() - 1 over a 4 MB adversarial stream fed byte by byte.
TEST(ScannerBoundedMemory4MB) {
    const std::size_t kBytes = 4u * 1024u * 1024u;
    std::string input;
    input.reserve(kBytes + 64);
    while (input.size() < kBytes) {
        input += "<|end_conversation|";  // one character short of the sentinel
        input += "<|end_";               // repeated partial prefixes
        input += "x<";
    }

    SentinelScanner sc(kSentinel);
    const std::size_t bound = kSentinel.size() - 1;
    std::string out;
    out.reserve(input.size());
    std::size_t max_held = 0;

    for (std::size_t i = 0; i < input.size(); ++i) {
        auto r = sc.feed(std::string_view(&input[i], 1));
        assert(!r.sentinel_found);
        out += r.safe_text;
        const std::size_t held = (i + 1) - out.size();  // == pending_.size()
        if (held > max_held) max_held = held;
        assert(held <= bound);
    }
    assert(max_held <= bound);
    assert(max_held > 0);  // the stream really did exercise the buffer

    auto f = sc.flush();
    assert(f.safe_text.size() == input.size() - out.size());  // flush releases exactly the held bytes
    out += f.safe_text;
    assert(out == input);  // nothing lost, nothing invented
    auto again = sc.flush();
    assert(again.safe_text.empty());
}

TEST(ScannerFlushAndEdgeCases) {
    SentinelScanner sc(kSentinel);
    auto a = sc.feed("ok<|end_");
    assert(a.safe_text == "ok" && !a.sentinel_found);  // partial sentinel held back
    auto f = sc.flush();
    assert(f.safe_text == "<|end_" && !f.sentinel_found);  // released at end of stream
    assert(sc.flush().safe_text.empty());

    // An empty chunk in the middle of a split sentinel changes nothing.
    SentinelScanner sc2(kSentinel);
    ScanResult r;
    feed_into(sc2, "x<|end_", r);
    feed_into(sc2, "", r);
    feed_into(sc2, "conversation|>", r);
    assert(r.found && r.text == "x");

    bool threw = false;
    try {
        SentinelScanner bad("");
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);
}

// ============================================================================
// Harness (provided loop) running on my Conversation and SentinelScanner
// ============================================================================

// Spec 10
TEST(HarnessTurnLimit) {
    TempFile script("p2_turn_limit.script",
                    "chunk: 2\nrole: assistant\nOne.\n---\n"
                    "chunk: 2\nrole: assistant\nTwo.\n---\n"
                    "chunk: 2\nrole: assistant\nThree.\n");
    Harness h = make_scripted_harness(script.path(), 2);
    LineInput in("a\nb\nc\n");
    CollectOutput out;

    StopReason reason = h.run(in, out);
    assert(reason.kind == StopReason::Kind::TurnLimit);
    assert(in.lines_read() == 2);  // the third line was never consumed

    const Conversation& c = h.conversation();
    assert(c.size() == 4);
    assert(c.at(0).role() == Role::User && c.at(0).content() == "a");
    assert(c.at(1).role() == Role::Assistant && c.at(1).content() == "One.");
    assert(c.at(2).role() == Role::User && c.at(2).content() == "b");
    assert(c.at(3).role() == Role::Assistant && c.at(3).content() == "Two.");
    assert(out.text().find("Three.") == std::string::npos);
}

// Spec 11: the sentinel is split across chunk boundaries (chunk: 3).
TEST(HarnessSentinelHalt) {
    TempFile script("p2_sentinel_halt.script",
                    "role: system\nBe brief.\n---\n"
                    "chunk: 3\nrole: assistant\nBye now." + kSentinel + "\n---\n"
                    "role: assistant\nSHOULD NEVER BE REACHED\n");
    Harness h = make_scripted_harness(script.path(), 20);
    LineInput in("hi\nagain\n");
    CollectOutput out;

    StopReason reason = h.run(in, out);
    assert(reason.kind == StopReason::Kind::Sentinel);
    assert(in.lines_read() == 1);  // halted right after that reply

    // The terminal never shows any part of the sentinel...
    assert(out.text().find("Bye now.") != std::string::npos);
    assert(out.text().find("<|") == std::string::npos);
    assert(out.text().find("end_conversation") == std::string::npos);
    assert(out.text().find("SHOULD NEVER") == std::string::npos);

    // ...but the stored message keeps it, so a saved transcript replays the stop.
    const Conversation& c = h.conversation();
    assert(c.size() == 3);
    assert(c.at(2).role() == Role::Assistant);
    assert(c.at(2).content() == "Bye now." + kSentinel);
}

TEST(HarnessEofExit) {
    TempFile script("p2_eof.script", "role: assistant\nHello there.\n---\nrole: assistant\nMore.\n");

    {
        Harness h = make_scripted_harness(script.path(), 20);
        LineInput in("");  // EOF immediately
        CollectOutput out;
        StopReason reason = h.run(in, out);
        assert(reason.kind == StopReason::Kind::UserExit);
        assert(h.conversation().size() == 0);
        assert(h.conversation().begin() == h.conversation().end());
    }
    {
        Harness h = make_scripted_harness(script.path(), 20);
        LineInput in("hi\n");  // one turn, then EOF
        CollectOutput out;
        StopReason reason = h.run(in, out);
        assert(reason.kind == StopReason::Kind::UserExit);
        assert(h.conversation().size() == 2);
        assert(h.conversation().at(1).content() == "Hello there.");
    }
}

// Spec 2 (harness level)
TEST(HarnessSystemMessagePinned) {
    TempFile script("p2_system.script",
                    "role: system\nBe concise.\n---\n"
                    "chunk: 4\nrole: assistant\nFirst.\n---\n"
                    "chunk: 4\nrole: assistant\nSecond.\n");
    Harness h = make_scripted_harness(script.path(), 2);
    LineInput in("x\ny\n");
    CollectOutput out;
    h.run(in, out);

    const Conversation& c = h.conversation();
    assert(c.size() == 5);
    const Role expected[] = {Role::System, Role::User, Role::Assistant, Role::User, Role::Assistant};
    for (std::size_t i = 0; i < 5; ++i) assert(c.at(i).role() == expected[i]);
    assert(c.at(0).content() == "Be concise.");
}

// Spec 12
TEST(TranscriptRoundTrip) {
    TempFile script("p2_roundtrip.script",
                    "role: system\nStay calm.\n---\n"
                    "chunk: 4\nrole: assistant\nFirst reply.\n---\n"
                    "chunk: 5\nrole: assistant\nSecond reply.\n---\n"
                    "chunk: 3\nrole: assistant\nDone!" + kSentinel + "\n");
    const std::string inputs = "one\ntwo\nthree\n";

    // Run 1: scripted, then save the conversation as a transcript.
    Harness first = make_scripted_harness(script.path(), 20);
    LineInput in1(inputs);
    CollectOutput out1;
    StopReason r1 = first.run(in1, out1);
    assert(r1.kind == StopReason::Kind::Sentinel);

    TempFile transcript("p2_roundtrip_transcript.txt", "");
    save_transcript(first.conversation(), transcript.path());

    // Run 2: replay that transcript with the same user inputs.
    auto replay = std::make_unique<ReplayModelClient>(transcript.path());
    HarnessConfig cfg;
    cfg.system_message = replay->system_message();
    Harness second(std::move(replay), cfg);
    LineInput in2(inputs);
    CollectOutput out2;
    StopReason r2 = second.run(in2, out2);

    assert(r2.kind == r1.kind);
    assert(r2.detail == r1.detail);
    assert(out2.text() == out1.text());  // identical playback on screen
    assert(same_contents(second.conversation(), first.conversation()));
    assert(second.conversation().size() == 7);
    assert(second.conversation().at(6).content() == "Done!" + kSentinel);
}

int main() {
    // Conversation
    RUN(EmptyConversationBounds);
    RUN(AtIsBoundsChecked);
    RUN(SystemMessageStaysPinned);
    RUN(CopyConstructorIsDeep);
    RUN(CopyAssignmentIsDeep);
    RUN(MoveConstructorStealsAndZeroes);
    RUN(MoveAssignmentStealsAndZeroes);
    RUN(GrowthDoublesAndPreservesContents);
    // SentinelScanner
    RUN(ScannerCleanText);
    RUN(ScannerChunkSizeIndependence);
    RUN(ScannerSplitAtEveryBoundary);
    RUN(ScannerThreeWaySplits);
    RUN(ScannerOneCharAtATime);
    RUN(ScannerFalseAlarms);
    RUN(ScannerSelfOverlappingSentinel);
    RUN(ScannerBoundedMemory4MB);
    RUN(ScannerFlushAndEdgeCases);
    // Harness
    RUN(HarnessTurnLimit);
    RUN(HarnessSentinelHalt);
    RUN(HarnessEofExit);
    RUN(HarnessSystemMessagePinned);
    RUN(TranscriptRoundTrip);

    std::cout << "\nAll tests passed.\n";
    return 0;
}
