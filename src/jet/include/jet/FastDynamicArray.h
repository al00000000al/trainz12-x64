// Jet::FastDynamicArray<T, SIZ, Less>: Jet's chunked array, as TS12's modules see it.
//
// Memory layout must match the original exactly, because other modules walk these arrays with inlined code:
//   array    = { Chunk* head }                                   (4 bytes)
//   Chunk    = { Chunk* next; Chunk* prev; uint32 count; T data[SIZ] }   (a ring; 0x40 bytes for 13 pointers)
//   Iterator = { T* node; Chunk* head; Chunk* chunk }            (End() is all null)
// Chunks come from Jet::Alloc so any module may free them. Behaviour follows TS12's Kernel_7c instance
// (FastDynamicArray<PluginConfig*,13,Less<...>>): the sorted Insert/InsertUnique find the first chunk whose
// last item is >= the new one and BinarySearch inside it (0x10008ca0).
#pragma once

#include <jet/KernelStandard.h>

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <new>

namespace Jet {

template <class A, class B>
struct Less {
    bool operator()(const A& a, const B& b) const { return a < b; }
};

template <class T, unsigned int SIZ = 13, class L = Less<T, T>>
class FastDynamicArray {
public:
    struct Chunk {
        Chunk* next;
        Chunk* prev;
        uint32_t count;
        T data[SIZ];
    };

    class ConstIterator;

    class Iterator {
    public:
        Iterator() : node(nullptr), head(nullptr), chunk(nullptr) {}
        Iterator(Chunk* h, Chunk* c) : node(c ? &c->data[0] : nullptr), head(h), chunk(c) {}
        Iterator(T* n, Chunk* h, Chunk* c) : node(n), head(h), chunk(c) {}
        bool operator==(const Iterator& o) const { return node == o.node; }
        bool operator!=(const Iterator& o) const { return node != o.node; }
        T& operator*() const { return *node; }
        Iterator& operator++()
        {
            if (node && ++node >= &chunk->data[chunk->count]) {
                chunk = chunk->next;
                node = (chunk != head) ? &chunk->data[0] : nullptr;
            }
            return *this;
        }
        Iterator& operator+=(int n)
        {
            while (n--) ++*this;
            return *this;
        }

        T* node;
        Chunk* head;
        Chunk* chunk;
    };

    class ConstIterator {
    public:
        ConstIterator() : node(nullptr), head(nullptr), chunk(nullptr) {}
        ConstIterator(Chunk* h, Chunk* c) : node(c ? &c->data[0] : nullptr), head(h), chunk(c) {}
        ConstIterator(const T* n, Chunk* h, Chunk* c) : node(n), head(h), chunk(c) {}
        ConstIterator(const Iterator& i) : node(i.node), head(i.head), chunk(i.chunk) {}
        bool operator==(const ConstIterator& o) const { return node == o.node; }
        bool operator!=(const ConstIterator& o) const { return node != o.node; }
        const T& operator*() const { return *node; }
        ConstIterator& operator++()
        {
            if (node && ++node >= &chunk->data[chunk->count]) {
                chunk = chunk->next;
                node = (chunk != head) ? &chunk->data[0] : nullptr;
            }
            return *this;
        }
        ConstIterator& operator+=(int n)
        {
            while (n--) ++*this;
            return *this;
        }

        const T* node;
        Chunk* head;
        Chunk* chunk;
    };

    FastDynamicArray() : head(nullptr) {}
    ~FastDynamicArray() { RemoveAll(); }
    FastDynamicArray(const FastDynamicArray& o) : head(nullptr) { *this += o; }
    FastDynamicArray& operator=(const FastDynamicArray& o)
    {
        if (this != &o) {
            RemoveAll();
            *this += o;
        }
        return *this;
    }

    FastDynamicArray& operator+=(const FastDynamicArray& o)
    {
        for (ConstIterator i = o.First(); i != o.End(); ++i) Add(*i);
        return *this;
    }

    // copies `o` chunk by chunk (each chunk keeps its fill) after the last chunk of this array; trivially
    // copyable items only (InputSystem's ObserverSubject copy, 0x1000c580: count and items by memcpy)
    void AppendChunks(const FastDynamicArray& o)
    {
        Chunk* src = o.head;
        if (!src) return;
        Chunk* tail = head ? head->prev : nullptr;
        do {
            Chunk* c = NewChunk();
            if (!head) {
                head = c;
                c->next = c->prev = c;
            } else {
                c->next = head;
                c->prev = tail;
                head->prev = c;
                tail->next = c;
            }
            std::memcpy(&c->count, &src->count, sizeof(uint32_t) + src->count * sizeof(T));
            tail = c;
            src = src->next;
        } while (src != o.head);
    }

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
        tail->data[tail->count++] = item;
    }

    void AddPack(const T& item)
    {
        Chunk* c;
        if (!head) {
            c = head = NewChunk();
            head->prev = head->next = head;
        } else {
            c = head;
            for (;;) {
                if (c->count < SIZ) break;
                if (c->next == head) {
                    Chunk* n = NewChunk();
                    n->next = head;
                    n->prev = c;
                    head->prev = n;
                    c->next = n;
                    c = n;
                    break;
                }
                c = c->next;
            }
        }
        c->data[c->count++] = item;
    }

    void AddUnique(const T& item)
    {
        if (!Find(item).node) Add(item);
    }

    void Push(const T& item)
    {
        if (!head) {
            head = NewChunk();
            head->prev = head->next = head;
        } else if (head->count == SIZ) {
            Chunk* c = NewChunk();
            c->next = head;
            c->prev = head->prev;
            head->prev->next = c;
            head->prev = c;
            head = c;
        } else {
            std::memmove(&head->data[1], &head->data[0], head->count * sizeof(T));
        }
        ++head->count;
        head->data[0] = item;
    }

    void PushUnique(const T& item)
    {
        if (!Find(item).node) Push(item);
    }

    // 0x10008190
    Iterator Insert(const T& item)
    {
        Chunk* const first = head;
        if (!first) {
            Add(item);
            return Iterator(head, head);
        }
        Chunk* c = first;
        for (;;) {
            if (!L()(c->data[c->count - 1], item)) {
                bool found;
                const unsigned int at = BinarySearch(c, item, found);
                return InsertBefore(Iterator(&c->data[at], first, c), item);
            }
            c = c->next;
            if (c == first) break;
        }
        Add(item);
        return Last();
    }
    Iterator Insert(const Iterator& before, const T& item) { return InsertBefore(before, item); }

    // 0x10008490
    Iterator InsertUnique(const T& item)
    {
        Chunk* const first = head;
        if (!first) {
            Add(item);
            return Iterator(head, head);
        }
        Chunk* c = first;
        for (;;) {
            if (!L()(c->data[c->count - 1], item)) {
                bool found;
                const unsigned int at = BinarySearch(c, item, found);
                if (found) return End();
                return InsertBefore(Iterator(&c->data[at], first, c), item);
            }
            c = c->next;
            if (c == first) break;
        }
        Add(item);
        return Last();
    }

    Iterator InsertAfter(const Iterator& after, const T& item)
    {
        Iterator i(after);
        return InsertBefore(++i, item);
    }

    Iterator InsertBefore(const Iterator& before, const T& item)
    {
        if (before == End()) {
            Add(item);
            return Last();
        }
        if (before == First()) {
            Push(item);
            return First();
        }
        Chunk* c = before.chunk;
        uint32_t index = static_cast<uint32_t>(before.node - &c->data[0]);
        if (index == 0 && c->count == SIZ) {
            Chunk* prev = c->prev;
            if (prev->count == SIZ || c == head) {
                Chunk* n = NewChunk();
                n->next = c;
                n->prev = c->prev;
                c->prev->next = n;
                c->prev = n;
                prev = n;
            }
            prev->data[prev->count++] = item;
            return Iterator(&prev->data[prev->count - 1], head, prev);
        }
        if (c->count == SIZ) {
            Chunk* n = NewChunk();
            n->prev = c;
            n->next = c->next;
            c->next->prev = n;
            c->next = n;
            std::memmove(&n->data[0], &c->data[SIZ / 2], (SIZ - SIZ / 2) * sizeof(T));
            c->count = SIZ / 2;
            n->count = SIZ - SIZ / 2;
            if (index > SIZ / 2) {
                c = n;
                index -= SIZ / 2;
            }
        }
        std::memmove(&c->data[index + 1], &c->data[index], (c->count - index) * sizeof(T));
        c->data[index] = item;
        ++c->count;
        return Iterator(&c->data[index], head, c);
    }

    // 0x10008690. The original reads the first item even when empty (a null read); we return T().
    T Pop()
    {
        if (!head) return T();
        T v = head->data[0];
        Remove(First());
        return v;
    }

    // removes and returns the last item
    T Remove()
    {
        if (!head) return T();
        Chunk* c = head->prev;
        T v = c->data[--c->count];
        if (!c->count) {
            if (c == head) {
                FreeChunk(c);
                head = nullptr;
            } else {
                Chunk* p = c->prev;
                p->next = head;
                head->prev = p;
                FreeChunk(c);
            }
        }
        return v;
    }

    Iterator Remove(const Iterator& i)
    {
        if (!head || i == End()) return i;
        Chunk* c = i.chunk;
        T* node = i.node;
        T* last = &c->data[c->count - 1];
        if (node < last) std::memmove(node, node + 1, (last - node) * sizeof(T));
        if (c->count == 1) {
            if (c == head) {
                if (c->next == head) {
                    FreeChunk(c);
                    head = nullptr;
                    node = nullptr;
                    c = nullptr;
                } else {
                    c->prev->next = c->next;
                    c->next->prev = c->prev;
                    Chunk* t = c;
                    c = c->next;
                    FreeChunk(t);
                    node = &c->data[0];
                    head = c;
                }
            } else {
                c->prev->next = c->next;
                c->next->prev = c->prev;
                Chunk* t = c;
                c = c->next;
                FreeChunk(t);
                node = (c == head) ? nullptr : &c->data[0];
            }
        } else {
            --c->count;
            if (node == last) {
                c = c->next;
                node = (c == head) ? nullptr : &c->data[0];
            }
        }
        return Iterator(node, head, c);
    }

    Iterator Remove(unsigned long n)
    {
        Iterator i = First();
        i += static_cast<int>(n);
        return Remove(i);
    }

    void RemoveLast(unsigned long n)
    {
        if (!head) return;
        Chunk* last = head->prev;
        while (last != head && last->count <= n) {
            n -= last->count;
            last = last->prev;
        }
        if (n >= last->count) {
            RemoveAll();
            return;
        }
        if (n) last->count -= n;
        Chunk* c = last->next;
        while (c != head) {
            Chunk* next = c->next;
            FreeChunk(c);
            c = next;
        }
        last->next = head;
        head->prev = last;
    }

    Iterator RemovePack(const Iterator& i)
    {
        Chunk* c = i.chunk;
        if (!head || i == End()) return i;
        if (c->count == 1) {
            if (head == c) {
                if (c->next == head) {
                    FreeChunk(head);
                    head = nullptr;
                    return End();
                }
                // TS12 builds the result after moving the head (the SDK kept the freed head in it)
                head = c->next;
                c->prev->next = c->next;
                c->next->prev = c->prev;
                FreeChunk(c);
                return Iterator(head, head);
            }
            Iterator n(head, c->next);
            c->prev->next = c->next;
            c->next->prev = c->prev;
            FreeChunk(c);
            return n;
        }
        T* node = i.node;
        --c->count;
        if (node != &c->data[c->count]) {
            *node = c->data[c->count];
            return i;
        }
        return Iterator(head, c->next);
    }

    void RemoveAll()
    {
        if (!head) return;
        Chunk* c = head->next;
        while (c != head) {
            Chunk* next = c->next;
            FreeChunk(c);
            c = next;
        }
        FreeChunk(c);
        head = nullptr;
    }

    T& operator[](unsigned long index) { return *(First() += static_cast<int>(index)); }
    const T& operator[](unsigned long index) const { return *(First() += static_cast<int>(index)); }

    Iterator First() { return Iterator(head, head); }
    ConstIterator First() const { return ConstIterator(head, head); }
    Iterator Last()
    {
        if (head) return Iterator(&head->prev->data[head->prev->count - 1], head, head->prev);
        return End();
    }
    ConstIterator Last() const
    {
        if (head) return ConstIterator(&head->prev->data[head->prev->count - 1], head, head->prev);
        return End();
    }
    Iterator End() { return Iterator(); }
    ConstIterator End() const { return ConstIterator(); }

    // 0x10008bb0: no bounds check (unlike the SDK); past the end this gives {null, head, head}
    Iterator Index(unsigned long n) { return First() += static_cast<int>(n); }
    ConstIterator Index(unsigned long n) const { return First() += static_cast<int>(n); }

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
    bool IsEmpty() const { return head == nullptr; }
    explicit operator bool() const { return head != nullptr; }

    unsigned long Index(Iterator i) const { return Index(ConstIterator(i)); }
    // 0x10008b20: items in the chunks before i's chunk + i's offset in it. Like the original this yields
    // -3 for End() of an empty array and nonsense for {null, head, head}.
    unsigned long Index(ConstIterator i) const
    {
        Chunk* c = head;
        unsigned long n = 0;
        if (i.chunk != c) {
            do {
                n += c->count;
                c = c->next;
                if (c == head) return n;
            } while (i.chunk != c);
        }
        // 32-bit: (node - chunk - 0xc) >> 2. Kept as address arithmetic (not node - c->data) because c and node
        // may be null; every address involved is item-aligned, so the division is exact.
        const intptr_t offset = (reinterpret_cast<intptr_t>(i.node) - reinterpret_cast<intptr_t>(c) -
                                 static_cast<intptr_t>(offsetof(Chunk, data))) /
                                static_cast<intptr_t>(sizeof(T));
        return n + static_cast<unsigned long>(offset);
    }

    // 0x10008c20
    void Swap(Iterator& a, Iterator& b)
    {
        if (a.node && b.node) {
            T t = *a.node;
            *a.node = *b.node;
            *b.node = t;
        }
    }

    // 0x10008c50
    bool Exists(const Iterator& i) const { return i.node && Find(*i.node).node; }

    // in place, ascending; chunks keep their sizes
    void Sort()
    {
        const unsigned long n = Count();
        for (unsigned long i = 1; i < n; ++i) {
            for (unsigned long j = i; j > 0 && L()((*this)[j], (*this)[j - 1]); --j) {
                T t = (*this)[j];
                (*this)[j] = (*this)[j - 1];
                (*this)[j - 1] = t;
            }
        }
    }

    ConstIterator Find(const T& item) const
    {
        for (ConstIterator i = First(); i != End(); ++i)
            if (*i == item) return i;
        return End();
    }
    Iterator Find(const T& item)
    {
        for (Iterator i = First(); i != End(); ++i)
            if (*i == item) return i;
        return End();
    }

    Chunk* head;

private:
    // 0x10008ca0: index of the first item not less than `key` in one chunk; `found` when it is equal
    unsigned int BinarySearch(Chunk* c, const T& key, bool& found) const
    {
        found = false;
        int lo = 0;
        int mid = static_cast<int>(c->count);
        if (mid > 0) {
            int hi = mid;
            do {
                mid = (hi + lo) / 2;
                if (L()(c->data[mid], key)) {
                    lo = mid + 1;
                    mid = hi;
                } else if (!L()(key, c->data[mid])) {
                    found = true;
                    if (mid <= lo) return static_cast<unsigned int>(mid);
                    do {
                        if (L()(c->data[mid - 1], key)) return static_cast<unsigned int>(mid);
                        --mid;
                    } while (lo < mid);
                    return static_cast<unsigned int>(mid);
                }
                hi = mid;
            } while (lo < mid);
        }
        return static_cast<unsigned int>(mid);
    }

    // every slot default-constructed (a no-op for pointers; InputSystem's observer slots clear their flags)
    static Chunk* NewChunk()
    {
        Chunk* c = static_cast<Chunk*>(Alloc::New(sizeof(Chunk)));
        for (unsigned int i = 0; i < SIZ; ++i) new (&c->data[i]) T;
        c->next = c->prev = nullptr;
        c->count = 0;
        return c;
    }
    static void FreeChunk(Chunk* c) { Alloc::Delete(c); }
};

}  // namespace Jet
