#pragma once

#include "common.h"
#include "memory.h"

namespace lox
{

template <class T>
class Array
{
  public:
    ~Array() {}

    Array() : count_(0), capacity_(0), elems_(NULL) {}

    virtual void init()
    {
        count_ = 0;
        capacity_ = 0;
        elems_ = NULL;
    }

    virtual void write(T elem)
    {
        if (capacity_ < count_ + 1)
        {
            int oldCapacity = capacity_;
            capacity_ = GROW_CAPACITY(oldCapacity);
            elems_ = GROW_ARRAY(T, elems_, oldCapacity, capacity_);
        }

        elems_[count_] = elem;
        count_++;
    }

    virtual void free()
    {
        FREE_ARRAY(T, elems_, capacity_);
        init();
    }

    int count() const { return count_; };
    int capacity() const { return capacity_; };
    const T* elems() const { return elems_; };

  private:
    int count_;
    int capacity_;
    T* elems_;
};

} // namespace lox
