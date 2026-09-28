// include/core/conversation.h
// YOURS — hand-rolled growable array of Message. The only class in this
// codebase allowed to use raw new[]/delete[]. Implements the full Rule
// of Five and grows with amortized O(1) append (doubling).

#pragma once
#include "message.h"
#include <cstddef>

class Conversation {
public:
    Conversation() = default;
    ~Conversation();

    // Rule of Five
    Conversation(const Conversation& other);
    Conversation& operator=(const Conversation& other);
    Conversation(Conversation&& other) noexcept;
    Conversation& operator=(Conversation&& other) noexcept;

    void append(Message m);

    std::size_t size() const noexcept { return size_; }
    std::size_t capacity() const noexcept { return capacity_; }  // extra: for growth tests

    // Bounds-checked access. Throws std::out_of_range if i >= size().
    const Message& at(std::size_t i) const;

    const Message* begin() const noexcept { return data_; }
    const Message* end()   const noexcept { return data_ + size_; }

private:
    static constexpr std::size_t kInitialCapacity = 4;

    Message*    data_ = nullptr;
    std::size_t size_ = 0;
    std::size_t capacity_ = 0;

    void grow();
};