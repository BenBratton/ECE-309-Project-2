// src/conversation.cpp

#include "core/conversation.h"
#include <stdexcept>
#include <utility>

Conversation::~Conversation() {
    delete[] data_;
}

// Deep copy: allocate our own array, then copy each live element over.
Conversation::Conversation(const Conversation& other)
    : data_(other.capacity_ ? new Message[other.capacity_] : nullptr),
      size_(other.size_),
      capacity_(other.capacity_) {
    for (std::size_t i = 0; i < size_; ++i) {
        data_[i] = other.data_[i];
    }
}

Conversation& Conversation::operator=(const Conversation& other) {
    if (this == &other) return *this;

    Message* new_data = other.capacity_ ? new Message[other.capacity_] : nullptr;
    for (std::size_t i = 0; i < other.size_; ++i) {
        new_data[i] = other.data_[i];
    }

    delete[] data_;
    data_ = new_data;
    size_ = other.size_;
    capacity_ = other.capacity_;
    return *this;
}

// Pointer-stealing move: take the other's buffer, leave it empty-but-valid.
Conversation::Conversation(Conversation&& other) noexcept
    : data_(other.data_), size_(other.size_), capacity_(other.capacity_) {
    other.data_ = nullptr;
    other.size_ = 0;
    other.capacity_ = 0;
}

Conversation& Conversation::operator=(Conversation&& other) noexcept {
    if (this == &other) return *this;

    delete[] data_;
    data_ = other.data_;
    size_ = other.size_;
    capacity_ = other.capacity_;

    other.data_ = nullptr;
    other.size_ = 0;
    other.capacity_ = 0;
    return *this;
}

void Conversation::grow() {
    std::size_t new_capacity = (capacity_ == 0) ? kInitialCapacity : capacity_ * 2;
    Message* new_data = new Message[new_capacity];
    for (std::size_t i = 0; i < size_; ++i) {
        new_data[i] = std::move(data_[i]);
    }
    delete[] data_;
    data_ = new_data;
    capacity_ = new_capacity;
}

void Conversation::append(Message msg) {
    if (size_ == capacity_) {
        grow();
    }
    data_[size_] = std::move(msg);
    ++size_;
}

const Message& Conversation::at(std::size_t i) const {
    if (i >= size_) {
        throw std::out_of_range("Conversation::at: index out of range");
    }
    return data_[i];
}