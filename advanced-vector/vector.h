/* разместите свой код в этом файле */
#pragma once

#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>
#include <algorithm>
#include <cstddef>
#include <type_traits>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;
    RawMemory(RawMemory&& other) noexcept
        : buffer_(other.buffer_)
        , capacity_(other.capacity_)  //
    {
        other.buffer_ = nullptr;
        other.capacity_ = 0;
    }
    RawMemory& operator=(RawMemory&& rhs) noexcept {
        Swap(rhs);
        return *this;
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:
    using iterator = T*;
    using const_iterator = const T*;

    Vector() noexcept = default;

    explicit Vector(size_t size)
        : data_(size)
        , size_(size)  //
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    Vector(const Vector& other)
        : data_(other.size_)
        , size_(other.size_)  //
    {
        std::uninitialized_copy_n(other.data_.GetAddress(), size_, data_.GetAddress());
    }

    Vector(Vector&& other) noexcept
        : data_(std::move(other.data_))
        , size_(other.size_)  //
    {
        other.size_ = 0;
    }

    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                /* Применить copy-and-swap */
                Vector rhs_copy(rhs);
                Swap(rhs_copy);
            }
            else {
                /* Скопировать элементы из rhs, создав при необходимости новые
                   или удалив существующие */
                size_t copy_count = std::min(size_, rhs.size_);
                std::copy_n(rhs.data_.GetAddress(), copy_count, data_.GetAddress());
                if (Size() < rhs.Size()) {
                    std::uninitialized_copy_n(rhs.data_ + size_, rhs.size_ - size_, data_ + size_);
                }
                else {
                    std::destroy_n(data_.GetAddress() + rhs.Size(), size_ - rhs.Size());
                }
                size_ = rhs.Size();
            }
        }
        return *this;
    }

    Vector& operator=(Vector&& rhs)  noexcept {
        if (this != &rhs) {
            //Vector rhs_copy(std::move(rhs));
            Swap(rhs);
        }
        return *this;
    }
    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    };

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }
        RawMemory<T> new_data(new_capacity);
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }

    void Resize(size_t new_size) {
        if (size_ > new_size) {
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
            size_ = new_size;
        }
        else {
            if (new_size > Capacity()) {
                RawMemory<T> new_data(new_size);
                if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                    for (size_t i = 0; i < size_; i++) {
                        new (new_data.GetAddress() + i) T(std::move(data_[i]));
                    }
                }
                else {
                    for (size_t i = 0; i < size_; i++) {
                        new (new_data.GetAddress() + i) T(data_[i]);
                    }
                }
                std::uninitialized_value_construct_n(new_data.GetAddress() + size_, new_size - size_);
                data_.Swap(new_data);
                std::destroy_n(new_data.GetAddress(), size_);
                size_ = new_size;
            }
            else {
                std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
                size_ = new_size;
            }
        }
    }

    void PushBack(const T& value) {
        EmplaceBack(value);
    }
    void PushBack(T&& value) {
        EmplaceBack(std::move(value));

    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        if (size_ < Capacity()) {
            new (data_ + size_) T(std::forward<Args>(args)...);
            ++size_;
            return *(data_ + (size_ - 1));
        }
        size_t new_capacity = (size_ == 0 ? 1 : size_ * 2);
        RawMemory<T> new_data(new_capacity);
        new (new_data + size_) T(std::forward<Args&&>(args)...);

        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        data_.Swap(new_data);
        std::destroy_n(new_data.GetAddress(), size_);
        size_++;
        return *(data_ + (size_ - 1));
    }

    iterator begin() noexcept {
        return data_.GetAddress();
    }
    iterator end() noexcept {
        return data_ + size_;
    }
    const_iterator begin() const noexcept {
        return data_.GetAddress();
    }
    const_iterator end() const noexcept {
        return data_ + size_;
    }
    const_iterator cbegin() const noexcept {
        return data_.GetAddress();
    }
    const_iterator cend() const noexcept {
        return data_ + size_;
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        if (size_ < Capacity()) {
            if (pos == end()) {
                new (end()) T(std::forward<Args&&>(args)...);
                ++size_;
                return &*(const_cast<iterator>(pos));
            }
            T temp(std::forward<Args&&>(args)...);
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(end() - 1, 1, end());
            }
            else {
                *(end()) = std::move(*(end() - 1));
            }
            iterator pos_end = end() - 1;
            std::move_backward(const_cast<iterator>(pos), pos_end, end());
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                *(const_cast<iterator>(pos)) = std::move(temp);
            }
            else {
                new(const_cast<iterator>(pos)) T(std::move(temp));

            }
            size_++;
            return const_cast<iterator>(pos);
        }
        else {
            size_t new_capacity = (size_ == 0 ? 1 : size_ * 2);
            RawMemory<T> new_data(new_capacity);
            new (new_data + (pos - begin())) T(std::forward<Args&&>(args)...);
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), pos - begin(), new_data.GetAddress());
                std::uninitialized_move_n(data_.GetAddress() + (pos - begin()), end() - pos, new_data.GetAddress() + (pos - begin() + 1));
            }
            else {
                std::uninitialized_copy_n(data_.GetAddress(), pos - begin(), new_data.GetAddress());
                std::uninitialized_copy_n(data_.GetAddress() + (pos - begin()), end() - pos, new_data.GetAddress() + (pos - begin() + 1));
            }
            data_.Swap(new_data);
            std::destroy_n(new_data.GetAddress(), size_);
            size_++;
            return data_ + (const_cast<iterator>(pos) - new_data.GetAddress());
        }
    }
    iterator Erase(const_iterator pos) noexcept/*(std::is_nothrow_move_assignable_v<T>);*/ {
        if (std::is_nothrow_move_assignable_v<T>) {
            for (iterator i = const_cast<iterator>(pos); i != end() - (pos - begin() + 1); i++) {
                *(i) = std::move(*(i + 1));
            }
        }
        else {
            for (iterator i = const_cast<iterator>(pos); i != end() - (pos - begin() + 1); i++) {
                *(i) = *(i + 1);
            }
        }
        size_--;
        end()->~T();
        return const_cast<iterator>(pos);
    }
    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }
    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::move(value));
    }

    void PopBack() noexcept {
        size_--;
        std::destroy_at(end());
    }

    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }

private:
    RawMemory<T> data_;
    size_t size_ = 0;
};



