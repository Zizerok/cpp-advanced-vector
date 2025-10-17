#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>
#include <cstdint> 

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
    RawMemory(RawMemory&& other) noexcept {
        buffer_ = other.buffer_;
        capacity_ = other.capacity_;

        other.buffer_ = nullptr;
        other.capacity_ = 0;
    }
    RawMemory& operator=(RawMemory&& rhs) noexcept { 
        if(this == &rhs){
            return *this;
        }
        Deallocate(buffer_);
        buffer_ = rhs.buffer_;
        capacity_ = rhs.capacity_;

        rhs.buffer_ = nullptr;
        rhs.capacity_ = 0;
        
        return *this;
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
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
    
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
}; 


template <typename T>
class Vector {
public:

    Vector() = default;

    Vector(size_t size) : data_(size), size_(size) {
    
        std::uninitialized_value_construct_n(data_.GetAddress(), size_);
    }

    Vector(const Vector& other) : data_(other.size_), size_(other.size_){

        std::uninitialized_copy_n(other.data_.GetAddress(), size_, data_.GetAddress());
    }

    Vector(Vector&& other) noexcept : data_(std::move(other.data_)), size_(other.size_) {
        other.size_ = 0;
    }

    Vector& operator=(const Vector& rhs){
        if(this == &rhs){
            return *this;
        }

        if(rhs.size_ > this->Capacity()){
            Vector rhs_temp(rhs);
            Swap(rhs_temp);
        } else{
            if(rhs.size_ >= size_){
                for(size_t i = 0; i < size_; i++ ){
                    data_[i] = rhs.data_[i];
                }
                for(size_t i = size_; i < rhs.size_; i++){
                    new(data_+i) T(rhs.data_[i]);
                }
            } else{
                for(size_t i = 0; i < rhs.size_; i++ ){
                    data_[i] = rhs.data_[i];
                }
                for(size_t i = rhs.size_; i < size_; i++){
                    Destroy(data_+i);
                }
            }
        }
        size_ = rhs.size_;
        return *this;
    }
    Vector& operator=(Vector&& rhs) noexcept {
    if (this != &rhs) {
        data_.Swap(rhs.data_);
        std::swap(size_, rhs.size_);
    }
    return *this;
}

    void Swap(Vector& other) noexcept{
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    ~Vector(){
        std::destroy_n(data_.GetAddress(), size_);
    }

    size_t Size() const noexcept {
        return size_;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    using iterator = T*;
    using const_iterator = const T*;
    
    iterator begin() noexcept{
        return data_.GetAddress();
    };

    iterator end() noexcept{
        return data_.GetAddress()+size_;
    };
    const_iterator begin() const noexcept{
        return data_.GetAddress();
    };
    const_iterator end() const noexcept{
        return data_.GetAddress()+size_;
    };
    const_iterator cbegin() const noexcept{
        return begin();
    };
    const_iterator cend() const noexcept{
        return end();
    };


template <typename... Args>
iterator Emplace(const_iterator pos, Args&&... args) {
    assert(pos >= begin() && pos <= end());
    size_t index = pos - begin();
    iterator result = nullptr;

    if (size_ == Capacity()) {
        RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
        new (new_data + index) T(std::forward<Args>(args)...);
        result = new_data + index;

        try {
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(begin(), index, new_data.GetAddress());
                std::uninitialized_move_n(begin() + index, size_ - index, new_data.GetAddress() + index + 1);
            } else {
                std::uninitialized_copy_n(begin(), index, new_data.GetAddress());
                std::uninitialized_copy_n(begin() + index, size_ - index, new_data.GetAddress() + index + 1);
            }
        } catch (...) {
            std::destroy_at(new_data + index);
            std::destroy_n(new_data.GetAddress(), index);
            throw;
        }

        std::destroy_n(begin(), size_);
        data_.Swap(new_data);
    } else {
        iterator mutable_pos = begin() + index;

        if (index == size_) {
            result = new (data_ + size_) T(std::forward<Args>(args)...);
        } else {
            new (data_ + size_) T(std::move(*(end() - 1))); 

            try {
                std::move_backward(mutable_pos, end() - 1, end());
                *mutable_pos = T(std::forward<Args>(args)...);
            } catch (...) {
                std::destroy_at(end()); 
                throw;
            }

            result = mutable_pos;
        }
    }

    ++size_;
    return result;
}




    iterator Erase(const_iterator pos) {
        assert(pos >= begin() && pos < end());
        size_t index = pos - begin();
        std::move(begin() + index + 1, end(), begin() + index);
        --size_;
        std::destroy_at(data_.GetAddress() + size_);
        return begin() + index;
    }

    iterator Insert(const_iterator pos, const T& value) {
    assert(pos >= begin() && pos <= end());
    size_t shift = pos - begin();

    const void* p = reinterpret_cast<const void*>(std::addressof(value));
    const void* buf_begin = reinterpret_cast<const void*>(data_.GetAddress() + shift);
    const void* buf_end   = reinterpret_cast<const void*>(data_.GetAddress() + size_);

    if (p >= buf_begin && p < buf_end) {
        T tmp = value;
        return Emplace(pos, std::move(tmp));
    } else {
        return Emplace(pos, value);
    }
}

iterator Insert(const_iterator pos, T&& value){
    return Emplace(pos, std::move(value));
}
    
    void Reserve(size_t capacity){
        if(capacity <= data_.Capacity()){
            return;
        }

        RawMemory<T> new_data(capacity);
        
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        } else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }

        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }


    size_t Capacity(){
        return data_.Capacity();
    }

    void Resize(size_t new_size){
        if(size_ > new_size){
            DestroyN(data_+new_size, size_ - new_size);  
        } else{
            if(new_size > Capacity()){
                Reserve(new_size);
                std::uninitialized_default_construct_n(data_ + size_, new_size - size_);
            } else{ 
                std::uninitialized_default_construct_n(data_ + size_, new_size - size_);
            }
        }
        size_ = new_size;
    };

    void PushBack(const T& value){
       EmplaceBack(value);
    }
    void PushBack(T&& value){
        EmplaceBack(std::move(value));
    }
    void PopBack(){
        assert(size_ != 0);

        Destroy(data_+size_-1);
        --size_;
    };

    template <typename... Args>
T& EmplaceBack(Args&&... args) {
    T* result = nullptr;

    if (size_ == Capacity()) {
        RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);

        result = new (new_data.GetAddress() + size_) T(std::forward<Args>(args)...);

        try {
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            } else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
        } catch (...) {
            std::destroy_at(new_data.GetAddress() + size_);
            throw;
        }
        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);

    } else {
        result = new (data_ + size_) T(std::forward<Args>(args)...);
    }

    ++size_;
    return *result;
}


private:
    static void CopyConstruct(T* buf, const T& elem) {
        new (buf) T(elem);
    }

    void Destroy(T* elem){
        elem->~T();
    }

    void DestroyN(T* elem, size_t n){
        for(size_t i = 0; i < n; i++){
            Destroy(elem+i);
        }
    }

    RawMemory<T> data_;
    size_t size_ = 0;
};
