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

template <class T, unsigned int SIZ = 13>
class Array {
public:
    Array() : count(0), capacity(0), data(nullptr) {}
    Array(const Array&) = delete;
    Array& operator=(const Array&) = delete;
    // destroys the items and frees the block (the pointer is left as is, like the original)
    ~Array()
    {
        for (unsigned long i = 0; i < count; ++i) data[i].~T();
        count = 0;
        if (data) Alloc::Delete(data);
    }

    // room for `needed` items: the block becomes ceil(needed / SIZ) * SIZ items, contents moved bytewise
    void Reserve(unsigned long needed)
    {
        if (needed <= capacity) return;
        const unsigned long items = (needed + SIZ - 1) / SIZ * SIZ;
        T* block = static_cast<T*>(Alloc::New(static_cast<unsigned int>(items * sizeof(T))));
        if (data) {
            std::memcpy(static_cast<void*>(block), data, count * sizeof(T));
            if (data) Alloc::Delete(data);
        }
        capacity = items;
        data = block;
    }

    unsigned long Count() const { return count; }
    unsigned long Capacity() const { return capacity; }
    const T& operator[](unsigned long i) const { return data[i]; }

    unsigned long count;     // +0
    unsigned long capacity;  // +4
    T* data;                 // +8
};

}  // namespace Jet
