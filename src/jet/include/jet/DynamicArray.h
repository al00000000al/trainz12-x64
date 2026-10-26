// Jet::DynamicArray<T, SIZ, Less> and Jet::Array<T, SIZ>: the containers of non-trivial items (PStrings, pairs of
// PStrings) that Kernel_7c's EditItem block hands out by reference and by value.
//
// DynamicArray has FastDynamicArray's memory layout (one Chunk* head; a ring of {next, prev, count, T data[SIZ]}
// chunks from Jet::Alloc), but builds its items in place and destroys them on removal. Only what TS12's Kernel_7c
// uses is here; behaviour follows its inlined copies (Add 0x10003120, RemoveAll 0x100031c0, Find 0x10008ec0, the
// chunk-wise append 0x10004300).
//
// Array is a flat block {count, capacity, T* data} that grows in steps of SIZ items (0x10003320).
#pragma once

#include <jet/FastDynamicArray.h>
#include <jet/KernelStandard.h>

#include <cstdint>
#include <cstring>
#include <new>

namespace Jet {

template <class T, unsigned int SIZ = 13, class L = Less<T, T>>
class DynamicArray {
public:
    struct Chunk {
        Chunk* next;
        Chunk* prev;
        uint32_t count;
        T data[SIZ];
    };

    class ConstIterator {
    public:
        ConstIterator() : node(nullptr), head(nullptr), chunk(nullptr) {}
        ConstIterator(const T* n, Chunk* h, Chunk* c) : node(n), head(h), chunk(c) {}
        bool operator==(const ConstIterator& o) const { return node == o.node; }
        bool operator!=(const ConstIterator& o) const { return node != o.node; }
        const T& operator*() const { return *node; }
        const T* operator->() const { return node; }
        ConstIterator& operator++()
        {
            if (node && ++node >= &chunk->data[chunk->count]) {
                chunk = chunk->next;
                node = (chunk != head) ? &chunk->data[0] : nullptr;
            }
            return *this;
        }

        const T* node;
        Chunk* head;
        Chunk* chunk;
    };

    DynamicArray() : head(nullptr) {}
    DynamicArray(const DynamicArray& o) : head(nullptr) { AppendChunks(o); }
    DynamicArray& operator=(const DynamicArray& o)
    {
        if (this != &o) {
            RemoveAll();
            AppendChunks(o);
        }
        return *this;
    }
    ~DynamicArray() { RemoveAll(); }

    // appends a copy at the end; a new chunk when the last one is full
    void Add(const T& item)
    {
        Chunk* tail;
        if (!head) {
            tail = head = NewChunk();
            head->prev = head->next = head;
        } else {
            tail = head->prev;
            if (tail->count == SIZ) {
                Chunk* c = NewChunk();
                c->next = head;
                c->prev = tail;
                head->prev = c;
                tail->next = c;
                tail = c;
            }
        }
        new (&tail->data[tail->count++]) T(item);
    }

    // detaches the ring first, then destroys every item and frees the chunks from the second one round to the head
    void RemoveAll()
    {
        Chunk* const first = head;
        if (!first) return;
        head = nullptr;
        Chunk* c = first->next;
        while (c != first) {
            Chunk* next = c->next;
            DestroyItems(c);
            Alloc::Delete(c);
            c = next;
        }
        DestroyItems(c);
        Alloc::Delete(c);
    }

    // copies `o` chunk by chunk (same chunk fill) after the last chunk of this array
    void AppendChunks(const DynamicArray& o)
    {
        Chunk* src = o.head;
        if (!src) return;
        do {
            Chunk* c = NewChunk();
            c->count = src->count;
            for (uint32_t i = 0; i < src->count; ++i) new (&c->data[i]) T(src->data[i]);
            if (!head) {
                head = c;
                c->next = c->prev = c;
            } else {
                Chunk* tail = head->prev;
                c->next = head;
                c->prev = tail;
                head->prev = c;
                tail->next = c;
            }
            src = src->next;
        } while (src != o.head);
    }

    // first item equal to `item` (the original compares the first slot of a chunk before its count)
    ConstIterator Find(const T& item) const
    {
        Chunk* c = head;
        if (!c) return ConstIterator();
        const T* n = &c->data[0];
        for (;;) {
            if (*n == item) return ConstIterator(n, head, c);
            if (++n >= &c->data[c->count]) {
                c = c->next;
                if (c == head) return ConstIterator();
                n = &c->data[0];
            }
        }
    }

    ConstIterator First() const { return ConstIterator(head ? &head->data[0] : nullptr, head, head); }
    ConstIterator End() const { return ConstIterator(); }
    unsigned long Count() const
    {
        unsigned long n = 0;
        if (Chunk* c = head) {
            do {
                n += c->count;
                c = c->next;
            } while (c != head);
        }
        return n;
    }

    Chunk* head;

private:
    static Chunk* NewChunk()
    {
        Chunk* c = static_cast<Chunk*>(Alloc::New(sizeof(Chunk)));
        c->count = 0;
        return c;
    }
    static void DestroyItems(Chunk* c)
    {
        for (uint32_t i = 0; i < c->count; ++i) c->data[i].~T();
    }
};

// The full member set follows RenderSystem's exported Array<PreProcessorDefine, 3> (0x1001cb10..0x1001d180); an
// iterator is {array, index}, null at the end.
template <class T, unsigned int SIZ = 13>
class Array {
public:
    class Iterator {
    public:
        Iterator(Array* a = nullptr, unsigned int i = 0) : array(a), index(i) {}
        Array* array;
        unsigned int index;
    };
    class ConstIterator {
    public:
        ConstIterator(const Array* a = nullptr, unsigned int i = 0) : array(a), index(i) {}
        const Array* array;
        unsigned int index;
    };

    Array() : count(0), capacity(0), data(nullptr) {}
    Array(const Array& o) : count(0), capacity(0), data(nullptr) { *this = o; }
    // destroys the items and frees the block (the pointer is left as is, like the original)
    ~Array()
    {
        RemoveAll();
        if (data) Jet::Alloc::Delete(data);
    }

    // the items are copied into a block of the other's capacity (kept when ours is large enough); nothing when
    // the other has no block
    Array& operator=(const Array& o)
    {
        RemoveAll();
        if (o.data) {
            if (capacity < o.capacity) {
                if (data) Jet::Alloc::Delete(data);
                data = static_cast<T*>(Jet::Alloc::New(static_cast<unsigned int>(o.capacity * sizeof(T))));
                capacity = o.capacity;
            }
            count = o.count;
            for (unsigned int i = 0; i < count; ++i) new (&data[i]) T(o.data[i]);
        }
        return *this;
    }

    // room for `needed` items: the block becomes ceil(needed / SIZ) * SIZ items, contents moved bytewise
    void Reserve(unsigned int needed)
    {
        if (needed <= capacity) return;
        const unsigned int items = (needed + SIZ - 1) / SIZ * SIZ;
        T* block = static_cast<T*>(Jet::Alloc::New(static_cast<unsigned int>(items * sizeof(T))));
        if (data) {
            std::memcpy(static_cast<void*>(block), data, count * sizeof(T));
            if (data) Jet::Alloc::Delete(data);
        }
        capacity = items;
        data = block;
    }
    void Grow(unsigned int n) { Reserve(count + n); }
    // gives back `n` items of capacity, destroying the items beyond it (as the original, an empty array with
    // room left destroys index 0xffffffff)
    void Shrink(unsigned int n)
    {
        if (n >= capacity) {
            RemoveAll();
            return;
        }
        capacity -= n;
        unsigned int i = count - 1;
        while (i >= capacity) {
            data[i].~T();
            --i;
        }
        count = i + 1;
        T* block = static_cast<T*>(Jet::Alloc::New(static_cast<unsigned int>(capacity * sizeof(T))));
        std::memcpy(static_cast<void*>(block), data, count * sizeof(T));
        if (data) Jet::Alloc::Delete(data);
        data = block;
    }

    unsigned int Count() const { return count; }
    unsigned int Size() const { return capacity; }
    unsigned long Capacity() const { return capacity; }
    bool IsEmpty() const { return count == 0; }
    operator bool() const { return count != 0; }
    T& operator[](unsigned int i) { return data[i]; }
    const T& operator[](unsigned int i) const { return data[i]; }

    void SetCount(unsigned int n)
    {
        Reserve(n);
        while (count < n) {
            new (&data[count]) T();
            ++count;
        }
        while (count != n) {
            --count;
            data[count].~T();
        }
    }
    void Add(const T* items, unsigned int n)
    {
        Reserve(count + n);
        for (; n != 0; --n) {
            new (&data[count]) T(*items);
            ++count;
            ++items;
        }
    }
    void Add(const T& item) { Add(&item, 1); }
    int AddEmpty(unsigned int n)  // the index of the first new item
    {
        const unsigned int first = count;
        SetCount(count + n);
        return static_cast<int>(first);
    }
    T* Alloc(unsigned int n)
    {
        const unsigned int first = count;
        SetCount(count + n);
        return data + first;
    }
    T Remove()  // the last item
    {
        --count;
        T last(data[count]);
        data[count].~T();
        return last;
    }
    // the item at `it`, the rest moved down; `it` again while it is still inside, else the end
    Iterator Remove(Iterator& it)
    {
        if (it.index < count) {
            --count;
            data[it.index].~T();
            std::memmove(static_cast<void*>(data + it.index), data + it.index + 1, (count - it.index) * sizeof(T));
        }
        if (it.index < count) return it;
        return Iterator();
    }
    void RemoveFast(unsigned int i)  // the last item moved into its place
    {
        --count;
        data[i].~T();
        if (i < count) std::memcpy(static_cast<void*>(data + i), data + count, sizeof(T));
    }
    void RemoveAll()
    {
        for (unsigned int i = 0; i < count; ++i) data[i].~T();
        count = 0;
    }

    Iterator First() { return count ? Iterator(this, 0) : Iterator(); }
    ConstIterator First() const { return count ? ConstIterator(this, 0) : ConstIterator(); }
    Iterator Last() { return count ? Iterator(this, count - 1) : Iterator(); }
    ConstIterator Last() const { return count ? ConstIterator(this, count - 1) : ConstIterator(); }
    Iterator End() { return Iterator(); }
    ConstIterator End() const { return ConstIterator(); }
    Iterator Index(unsigned int i) { return i < count ? Iterator(this, i) : Iterator(); }
    ConstIterator Index(unsigned int i) const { return i < count ? ConstIterator(this, i) : ConstIterator(); }

    unsigned long count;     // +0
    unsigned long capacity;  // +4
    T* data;                 // +8
};

}  // namespace Jet
