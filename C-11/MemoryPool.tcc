/*-
 * Copyright (c) 2013 Cosku Acay, http://www.coskuacay.com
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef MEMORY_BLOCK_TCC
#define MEMORY_BLOCK_TCC


// .tcc 通常被理解为：Template C++ Code 或 Template Compilation Component
// 这是一个专门用来存放 模板类或模板函数定义部分 的文件

// 为什么模板类不能像普通类一样写成 .cpp 文件?
// 因为模板代码必须在编译时展开，而不能单独编译。

// 那为什么不直接把所有实现写在 .h 里？
// 但这样：会让头文件又长又杂；会影响代码结构清晰度； 多个模板类混在一个 .h 文件里会变得混乱；
// 所以很多工程会选择把实现放在 .tcc、.inl、.impl.h 等文件中。

//.tcc 文件里干了什么?
// 实现了 .h 文件里声明的所有函数； 按照模板格式展开； 等待编译器根据用户传入的 T 去实例化。

// 内存分配策略：
// 每次从操作系统申请一个大块内存（默认 4096 字节）；
// 分割成多个 Slot_；
// 用一个链表保存释放的 slot 以供复用；
// 分配时优先复用，避免频繁 new/delete 系统调用。


template <typename T, size_t BlockSize>
inline typename MemoryPool<T, BlockSize>::size_type
MemoryPool<T, BlockSize>::padPointer(data_pointer_ p, size_type align)
const noexcept
{
  uintptr_t result = reinterpret_cast<uintptr_t>(p);
  return ((align - result) % align);
}

// padPointer 函数：对齐指针
// 作用：计算为了满足对齐要求，需要偏移多少字节。
// reinterpret_cast<uintptr_t>：将指针转成整数，以便进行位运算。
// 返回值是应偏移的字节数，使得地址能被 align 整除。



template <typename T, size_t BlockSize>
MemoryPool<T, BlockSize>::MemoryPool()
noexcept
{
  currentBlock_ = nullptr;
  currentSlot_ = nullptr;
  lastSlot_ = nullptr;
  freeSlots_ = nullptr;
}

// 初始化所有指针为空。


template <typename T, size_t BlockSize>
MemoryPool<T, BlockSize>::MemoryPool(const MemoryPool& memoryPool)
noexcept :
MemoryPool()
{}
// 清空原对象指针


template <typename T, size_t BlockSize>
MemoryPool<T, BlockSize>::MemoryPool(MemoryPool&& memoryPool)
noexcept
{
  currentBlock_ = memoryPool.currentBlock_;
  memoryPool.currentBlock_ = nullptr;
  currentSlot_ = memoryPool.currentSlot_;
  lastSlot_ = memoryPool.lastSlot_;
  freeSlots_ = memoryPool.freeSlots;
}

// 移动构造将另一个池的内存转移给当前对象

template <typename T, size_t BlockSize>
template<class U>
MemoryPool<T, BlockSize>::MemoryPool(const MemoryPool<U>& memoryPool)
noexcept :
MemoryPool()
{}

// 模板构造允许类型转换

template <typename T, size_t BlockSize>
MemoryPool<T, BlockSize>&
MemoryPool<T, BlockSize>::operator=(MemoryPool&& memoryPool)
noexcept
{
  if (this != &memoryPool)
  {
    std::swap(currentBlock_, memoryPool.currentBlock_);
    currentSlot_ = memoryPool.currentSlot_;
    lastSlot_ = memoryPool.lastSlot_;
    freeSlots_ = memoryPool.freeSlots;
  }
  return *this;
}

// 使用 std::swap 转移块指针，其他直接赋值。

// 返回自身引用，允许链式调用。

template <typename T, size_t BlockSize>
MemoryPool<T, BlockSize>::~MemoryPool()
noexcept
{
  slot_pointer_ curr = currentBlock_;
  while (curr != nullptr) {
    slot_pointer_ prev = curr->next;
    operator delete(reinterpret_cast<void*>(curr));
    curr = prev;
  }
}

// 析构函数
// 作用：释放所有已分配的内存块。
// 遍历 currentBlock_ 链表，逐个释放每个内存块。

template <typename T, size_t BlockSize>
inline typename MemoryPool<T, BlockSize>::pointer
MemoryPool<T, BlockSize>::address(reference x)
const noexcept
{
  return &x;
}

// 返回对象的地址，符合 STL 分配器接口规范。

template <typename T, size_t BlockSize>
inline typename MemoryPool<T, BlockSize>::const_pointer
MemoryPool<T, BlockSize>::address(const_reference x)
const noexcept
{
  return &x;
}



template <typename T, size_t BlockSize>
void
MemoryPool<T, BlockSize>::allocateBlock()
{
  // Allocate space for the new block and store a pointer to the previous one
  data_pointer_ newBlock = reinterpret_cast<data_pointer_>
                           (operator new(BlockSize));
  reinterpret_cast<slot_pointer_>(newBlock)->next = currentBlock_;
  currentBlock_ = reinterpret_cast<slot_pointer_>(newBlock);
  // Pad block body to staisfy the alignment requirements for elements
  data_pointer_ body = newBlock + sizeof(slot_pointer_);
  size_type bodyPadding = padPointer(body, alignof(slot_type_));
  currentSlot_ = reinterpret_cast<slot_pointer_>(body + bodyPadding);
  lastSlot_ = reinterpret_cast<slot_pointer_>
              (newBlock + BlockSize - sizeof(slot_type_) + 1);
}

// 分配一个 BlockSize 字节的新块；

// 前几个字节用来存储前一个 block 的指针（形成链表）；

// body 是实际分配对象的起始位置；

// padPointer 确保 body 对齐；

// 设置 currentSlot_ 和 lastSlot_，准备循环分配。

template <typename T, size_t BlockSize>
inline typename MemoryPool<T, BlockSize>::pointer
MemoryPool<T, BlockSize>::allocate(size_type n, const_pointer hint)
{
  if (freeSlots_ != nullptr) {
    pointer result = reinterpret_cast<pointer>(freeSlots_);
    freeSlots_ = freeSlots_->next;
    return result;
  }
  else {
    if (currentSlot_ >= lastSlot_)
      allocateBlock();
    return reinterpret_cast<pointer>(currentSlot_++);
  }
}

// 如果 freeSlots_ 不为空，说明有回收的空间，优先复用；

// 否则，从当前块分配新 slot；

// 如果当前块满了（currentSlot_ >= lastSlot_），则调用 allocateBlock()。


template <typename T, size_t BlockSize>
inline void
MemoryPool<T, BlockSize>::deallocate(pointer p, size_type n)
{
  if (p != nullptr) {
    reinterpret_cast<slot_pointer_>(p)->next = freeSlots_;
    freeSlots_ = reinterpret_cast<slot_pointer_>(p);
  }
}

// 把对象 p 转换为 slot_pointer_；

// 将其插入到 freeSlots_ 链表头部，下次可复用。

template <typename T, size_t BlockSize>
inline typename MemoryPool<T, BlockSize>::size_type
MemoryPool<T, BlockSize>::max_size()
const noexcept
{
  size_type maxBlocks = -1 / BlockSize;
  return (BlockSize - sizeof(data_pointer_)) / sizeof(slot_type_) * maxBlocks;
}

// 理论上最多可以分配多少对象；
// 计算方式考虑了 block 数量上限、block 实际可用容量。

template <typename T, size_t BlockSize>
template <class U, class... Args>
inline void
MemoryPool<T, BlockSize>::construct(U* p, Args&&... args)
{
  new (p) U (std::forward<Args>(args)...); 
  // 在地址 p 所指的内存上，用 args... 这个参数，原地构造一个类型为 T 的对象
}

// 使用完美转发，在指定位置上使用 placement new 构造对象。
//  为什么不能写 U* p = new U(args...)？
//  new 会：自动申请一块新内存 然后在这块内存上构造对象 返回构造好的指针
//  你已经提前手动分配了内存（用的是 allocate()），如果你再用普通 new 就等于重复分配内存 而且还可能导致内存管理混乱；

template <typename T, size_t BlockSize>
template <class U>
inline void
MemoryPool<T, BlockSize>::destroy(U* p)
{
  p->~U();
}

// 显式调用对象的析构函数。

template <typename T, size_t BlockSize>
template <class... Args>
inline typename MemoryPool<T, BlockSize>::pointer
MemoryPool<T, BlockSize>::newElement(Args&&... args)
{
  pointer result = allocate();
  construct<value_type>(result, std::forward<Args>(args)...);
  return result;
}

// newElement：封装了 allocate + construct；

template <typename T, size_t BlockSize>
inline void
MemoryPool<T, BlockSize>::deleteElement(pointer p)
{
  if (p != nullptr) {
    // placement new 中需要手动调用元素 T 的析构函数
    p->~value_type();
    // 归还内存
    deallocate(p);
  }
}

// deleteElement：封装了 destroy + deallocate；

#endif // MEMORY_BLOCK_TCC
