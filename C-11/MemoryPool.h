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

#ifndef MEMORY_POOL_H
#define MEMORY_POOL_H

#include <climits>   // 包含 CHAR_BIT 等整数限制常量
#include <cstddef>   // 包含 size_t, ptrdiff_t 等基础类型

//  我们的 .h 文件里干了什么？
// .h 文件里是“类的外部接口”
// 声明了函数接口，但没写实现；
// 最后通过 #include "MemoryPool.tcc" 把实现拼进来。

// .h 文件就像是容器的接口外壳 定义了这个模板类的结构和接口
// .tcc 文件则是“模板的函数实现仓库”，最后通过 #include 机制把它嵌进头文件，使得在编译时模板可以完整展开。

// 这正是 C++ 模板类封装与分离实现的“半抽象式设计”。


// template <typename T, typename Alloc = std::allocator<T>> class vector { ... };
// Alloc 是容器的第二个模板参数，是“内存分配器类型”，默认是 std::allocator<T>
// 这个 alloc 变量是在 vector 里被声明和使用的分配器对象，你只能通过传不同类型的 Alloc 来控制它行为，不改变它的使用方式

template <typename T, size_t BlockSize = 4096>     // BlockSize 是每次分配的内存块大小（默认 4096 字节） T 是要分配的元素类型
class MemoryPool
{
  public:
    /* Member types */
    // 接口规范（Allocator Requirements）
    typedef T               value_type;                // 代表我们要分配的对象类型
    typedef T*              pointer;                   // 指向对象的指针
    typedef T&              reference;                 // 对象的引用
    typedef const T*        const_pointer;             // 指向常量对象的指针
    typedef const T&        const_reference;           // 常量对象的引用
    typedef size_t          size_type;                 // 用来表示大小（元素数量）
    typedef ptrdiff_t       difference_type;           // 指针差值（地址之间的差）

    // 类型别名是为了符合 STL 分配器接口规范，例如容器在使用分配器时能统一接口

    typedef std::false_type propagate_on_container_copy_assignment;
    typedef std::true_type  propagate_on_container_move_assignment;
    typedef std::true_type  propagate_on_container_swap;

    // 这些类型用于告诉 STL 容器在赋值、移动、交换时是否应传播分配器
    // 用于告诉 STL 容器在某些操作（拷贝、移动、交换）时，要不要把分配器也一起传过去

    template <typename U> 
    struct rebind {
      typedef MemoryPool<U> other;
    };
    // STL 容器有时不仅需要为元素 T 分配内存，还可能要为 内部结构（如节点、辅助结构体） 分配内存。
    // 这时，它会调用你的分配器模板的“换类型功能”。实现类型的重新绑定

    // 模板结构体 定义在 MemoryPool<T> 这个类里面。
    // C++ 中的模板元编程（template metaprogramming）没有办法用函数来“返回类型”，
    // 只能用结构体 + typedef 的方式来做“类型计算”

    // 允许分配器在容器内部为不同类型重新生成一个分配器实例（如链表的节点类型和数据类型不同）。

    /* Member functions */
    /* 构造函数 */

    // noexcept 是 C++11 引入的关键字，意思是“我承诺这个函数不会抛出异常”
    // noexcept 修饰的函数可以被编译器优化（比如更适合移动构造）；
    // 如果函数标了 noexcept 却真的抛出异常，程序会直接 std::terminate() 崩溃！

    MemoryPool() noexcept;                                                     // 构造函数用于初始化内存池； // 这里只是声明 真正的实现是在 .tcc 文件里
    MemoryPool(const MemoryPool& memoryPool) noexcept;                         // 禁止拷贝赋值；  // 拷贝构造函数

    // 构造函数的名(不会修改原来的对象 参数是另一个 MemoryPool 对象的引用，用于“复制”  参数的变量名) 这个构造不会抛异常
    // & 表示引用，不会创建拷贝 提高效率

    MemoryPool(MemoryPool&& memoryPool) noexcept;                              // 允许移动赋值；  // 移动构造函数
    template <class U> MemoryPool(const MemoryPool<U>& memoryPool) noexcept;   // 支持从另一个类型的 MemoryPool<U> 构造；

    // MemoryPool&& 是一个右值引用（rvalue reference）类型  它是 C++11 新增的，用于支持“移动语义 和 MemoryPool&（左值引用）完全不同
    // MemoryPool&& 是一个完整的类型名 表示 我要拿走这个即将被销毁的对象的资源

    /* 析构函数 */
    ~MemoryPool() noexcept;                                                    // 析构函数会释放分配的内存块。

    MemoryPool& operator=(const MemoryPool& memoryPool) = delete;              // 运算符重载函数 表示“赋值运算”。
    MemoryPool& operator=(MemoryPool&& memoryPool) noexcept;
  
    // MemoryPool& 函数返回类型，表示赋值之后返回这个对象本身，方便链式赋值
    // operator= 这是运算符重载，重载的是 = 赋值操作符
    // (const MemoryPool& memoryPool) 表示赋值的来源是另一个 MemoryPool 对象
    //= delete 这是 C++11 的语法，表示你明确禁止使用这个函数！

    // MemoryPool 管理了复杂的资源（比如内存块），不适合被“复制赋值”，可能会导致双重释放、悬空指针等危险

    /* 元素取址 */
    pointer address(reference x) const noexcept;
    const_pointer address(const_reference x) const noexcept;                   // 返回对象的地址

    // Can only allocate one object at a time. n and hint are ignored
    // 分配和收回一个元素的内存空间
    pointer allocate(size_type n = 1, const_pointer hint = 0);                 // 分配 n 个元素（但实际上只支持 n = 1）
    void deallocate(pointer p, size_type n = 1);                               // 释放一个元素

    // 可达到的最多元素数
    size_type max_size() const noexcept;                                       // 返回可以分配的最大元素数量

    // 基于内存池的元素构造和析构
    // 在地址 p 上，使用你传入的参数 args...，调用 placement new 构造一个 U 类型对象。
    template <class U, class... Args> void construct(U* p, Args&&... args);    // construct：在内存上原地构造对象（placement new）；// U* p：构造的地址
    template <class U> void destroy(U* p);                                     // destroy：显式调用析构函数。不释放内存

    // template<class... Args> 可变模板参数:  我定义了一个模板 接受任意数量、任意类型的参数
    // Args&&... args  完美转发语法 结合了 可变参数（Args...） 引用折叠（&&）  转发（std::forward）
    // Args&&... args 意思是 我接收任意多个参数，保留它们的值/引用/右值属性，原样传下去

    // 自带申请内存和释放内存的构造和析构 
    template <class... Args> pointer newElement(Args&&... args);               // newElement 相当于分配 + 构造；
    void deleteElement(pointer p);                                             // deleteElement 相当于析构 + 回收。

  private:
    // union 结构体,用于存放元素或 next 指针
    union Slot_ {
      value_type element;                                                      // element：实际存储的对象；
      Slot_* next;                                                             // next：用于空闲链表连接的指针。 由于是 union，所以两个成员共享内存。
    }; 
    // 逻辑上更清晰：“使用中就是 element，空闲时就是 next”。
   
    // union所有成员“共用”同一块内存； 整个 union 的大小是它最大成员的大小； 任何时候，union 里只能“同时有效存在一个成员”。

    // 如果这块内存正在“使用”中，我们就把它当作 element；
    // 如果这块内存已经被“释放”回内存池，我们就把它当作链表节点，使用 next
    // 它用一块内存，既能装数据，又能当指针，完美复用了内存，节省了空间，也避免了重复管理两套结构

    typedef char* data_pointer_;       // char* 指针，主要用于指向内存首地址
    typedef Slot_ slot_type_;          // Slot_ 值类型
    typedef Slot_* slot_pointer_;      // Slot_* 指针类型                                         // 一些简化指针类型名，便于书写
 
    slot_pointer_ currentBlock_;       // 内存块链表的头指针(当前正在使用的内存块链表的头部)
    // 每次申请一个新 block，会从操作系统拿到一大块内存（BlockSize 大小）；每块内存里会切分成很多 Slot_；不同 block 会通过 Slot_::next 链接起来。

    slot_pointer_ currentSlot_;        // 元素链表的头指针(当前“可分配 slot”的位置。)
    slot_pointer_ lastSlot_;           // 可存放元素的最后指针(当前 block 的最后一个 slot)
    slot_pointer_ freeSlots_;          // 元素构造后释放掉的内存链表头指针                         // 这些指针用于管理内存块和回收的 slot
    // 用户 deleteElement(p) 的时候，这个 p（指向 Slot）会被塞回 freeSlots_ 
    // 下次分配时，会优先从 freeSlots_ 取，而不是从 currentSlot_

    size_type padPointer(data_pointer_ p, size_type align) const noexcept;     // padPointer：对齐指针到指定字节边界（一般为 alignof(T)）；
    void allocateBlock();                                                      // allocateBlock：当当前块用完时，申请一个新块。

    static_assert(BlockSize >= 2 * sizeof(slot_type_), "BlockSize too small.");// 静态检查内存块大小，必须能至少容纳两个 Slot_，否则报错。
};

#include "MemoryPool.tcc"

#endif // MEMORY_POOL_H


/*
MemoryPool<T> 管理：

[ currentBlock_ ] →  Block1 → Block2 → ...
                       |
                       v
           [Slot_0][Slot_1][Slot_2]...[Slot_n]
              ↑
      currentSlot_ 指向下一个可分配 Slot
              ↓
          lastSlot_ 是当前块最后的 Slot

[ freeSlots_ ] → SlotX → SlotY → SlotZ
        ↑
   回收后的 slot 用 next 指针组成链表

*/