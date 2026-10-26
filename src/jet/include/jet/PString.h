// Jet::PString and Jet::PStringCache (KernelStandard_7c). Interned, ref-counted strings.
//
// PString is one pointer to a cache node. Its constructors, copy and destructor are not exported: the original
// compiler inlined them into every module, so they are reconstructed here from those inlined copies
// (Kernel_7c Task::Task(const Task&) at 0x100010e0, trainz.exe FUN_009cfd80):
//   copy:    if (node) { cache->Lock(); ++node->refs (+0x10); ++cache->liveRefs (+4); cache->Unlock(); }
//   destroy: if (node && cache) cache->Destroy(node);
#pragma once

#include <jet/Platform.h>

#include <cstddef>

namespace Jet {

class BinReader;
class BinWriter;

class PStringCache {
public:
    // one interned string (KernelStandard 0x10039cc0): a list node sized (0x18 + length + 1) rounded up to 16
    struct Node {
        Node* next;             // +0x00  hash bucket chain
        Node* prev;             // +0x04
        unsigned int length;    // +0x08
        unsigned int capacity;  // +0x0c  bytes available for text
        int refs;               // +0x10
        unsigned int id;        // +0x14  zeroed on creation and by ClearIDs
        char text[1];           // +0x18  NUL-terminated
    };
    // a hash chain {head, tail, count}; new and found nodes go to the front
    struct Bucket {
        Node* head;
        Node* tail;
        int count;
    };

    // unit U14b (PString.cpp). The members are __cdecl with `this` on the stack, as exported.
    explicit PStringCache(unsigned int buckets);
    ~PStringCache();
    Node* __cdecl Alloc(unsigned int length);
    Node* __cdecl Create(const unsigned char* text);
    Node* __cdecl Create(const unsigned char* text, unsigned int length);
    Node* __cdecl Create(unsigned int ch, unsigned int count);  // count copies of ch
    Node* __cdecl CreateANSI(const unsigned char* text, int length);
    Node* __cdecl CreateUnicode(const __wchar_t* text, int length);  // (native wchar_t in the export names)
    Node* __cdecl Ins(Node* node, unsigned int ch, unsigned int pos, unsigned int count);  // count copies of ch
    Node* __cdecl Ins(Node* node, const unsigned char* text, unsigned int pos);
    Node* __cdecl Del(Node* node, unsigned int pos, unsigned int count);
    Node* __cdecl Set(Node* node, int value, unsigned int radix);
    Node* __cdecl Set(Node* node, const unsigned char* fmt, char* args);
    int __cdecl Find(Node* node, unsigned int ch, unsigned int start) const;  // index of ch from start, or -1
    int __cdecl Find(Node* node, const unsigned char* text, unsigned int start) const;
    Node* __cdecl Strip(Node* node, const unsigned char* chars);
    Node* __cdecl ToLower(Node* node);
    Node* __cdecl ToUpper(Node* node);
    void __cdecl Destroy(Node* node);
    int __cdecl GetANSI(Node* node, char* out, unsigned int size) const;
    int __cdecl GetUnicode(Node* node, __wchar_t* out, unsigned int size) const;
    unsigned int __cdecl GetHash(const unsigned char* text, unsigned int length) const;
    void __cdecl Lock();
    void __cdecl Unlock();
    Node* __cdecl GetUnique(Node* node, unsigned int length);  // a private copy of node (out of the cache)
    Node* __cdecl Insert(Node* node);                          // back in, merged with an equal one
    Node* __cdecl Next();  // the walk over every node that ClearIDs starts
    void __cdecl ClearIDs();
    void __cdecl CollectGarbage();  // frees the unreferenced nodes of the next bucket
    void __cdecl Dump();

    int usedNodes;      // +0x00  nodes with refs > 0
    int liveRefs;       // +0x04
    int hits;           // +0x08  lookups that found their text
    int nodes;          // +0x0c  nodes made
    int probes;         // +0x10  chain steps past a different text
    int unused14;       // +0x14
    int unused18;       // +0x18
    int bucketCount;    // +0x1c
    int gcBucket;       // +0x20  the last bucket CollectGarbage swept
    Bucket* buckets;    // +0x24  an array new'd from Jet::Alloc (count at -4)
    int walkBucket;     // +0x28
    Node* walkNode;     // +0x2c

private:
    Node* __cdecl Create(const unsigned char* text, unsigned int unused, unsigned int length);
};

class PString {
public:
    PString() : node(nullptr) {}
    // interned copy of `text` (PStringCache::Create hands back a node the caller owns)
    explicit PString(const char* text)
        : node(cache ? cache->Create(reinterpret_cast<const unsigned char*>(text)) : nullptr) {}
    bool operator==(const PString& o) const { return node == o.node; }  // interned: same text, same node
    bool operator!=(const PString& o) const { return node != o.node; }
    const char* Text() const { return node ? node->text : nullptr; }
    unsigned Length() const { return node ? node->length : 0; }
    PString(const PString& o) : node(o.node) { AddRef(); }
    // the node is left in place, as in the original: KernelResource finds a deleted resource's twin by the name
    // still sitting in its freed memory (ResourceDatabase::Remove, two resources of one name)
    ~PString()
    {
        if (node && cache) cache->Destroy(node);
    }
    PString& operator=(const PString& o)
    {
        if (node != o.node) {
            Release();
            node = o.node;
            AddRef();
        }
        return *this;
    }

    // unit U14b (PString.cpp)
    void __cdecl Setf(const char* fmt, ...);
    int __cdecl Add(const char* fmt, char* args);  // appends the formatted text; the count of characters formatted
    int __cdecl Addf(const char* fmt, ...);
    int __cdecl Compare(const PString& o) const;
    int __cdecl Compare(const char* text) const;
    int __cdecl CompareIgnoreCase(const PString& o) const;
    int __cdecl CompareIgnoreCase(const char* text) const;
    bool __cdecl EqualsIgnoreCase(const PString& o) const;
    bool __cdecl operator<(const PString& o) const;
    bool __cdecl operator>(const PString& o) const;
    bool __cdecl Read(BinReader& r);  // u32 header: bit 30 = UTF-16 byte count, else an ANSI length
    bool __cdecl Write(BinWriter& w) const;
    static int __cdecl SortPtr(const void* a, const void* b);  // by node address

    static PStringCache* Cache() { return cache; }

    PStringCache::Node* node;

private:
    void AddRef()
    {
        if (node) {
            cache->Lock();
            ++node->refs;
            ++cache->liveRefs;
            cache->Unlock();
        }
    }
    void Release()
    {
        if (node && cache) cache->Destroy(node);
        node = nullptr;
    }

    static JET_KS_FORWARDED PStringCache* cache;  // ?cache@PString@Jet@@0PAVPStringCache@2@A
    static JET_KS_FORWARDED bool isFinished;      // set when the cache is gone at shutdown

    friend struct FoundationWork;  // KernelStandardInit makes the cache, its shutdown routine destroys it
};

JET_LAYOUT32(sizeof(PString) == 4);
JET_LAYOUT32(sizeof(PStringCache) == 0x30);
JET_LAYOUT32(offsetof(PStringCache::Node, refs) == 0x10 && offsetof(PStringCache::Node, text) == 0x18);

}  // namespace Jet
