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
        unsigned int unknown;   // +0x14  zeroed on creation
        char text[1];           // +0x18  NUL-terminated
    };

    Node* __cdecl Create(const unsigned char* text);
    Node* __cdecl Create(const unsigned char* text, unsigned int length);
    Node* __cdecl CreateANSI(const unsigned char* text, int length);
    Node* __cdecl CreateUnicode(const __wchar_t* text, int length);  // (native wchar_t in the export names)
    Node* __cdecl Ins(Node* node, const unsigned char* text, unsigned int pos);
    int __cdecl Find(Node* node, unsigned int ch, unsigned int start) const;  // index of ch from start, or -1
    Node* __cdecl Strip(Node* node, const unsigned char* chars);
    void __cdecl Destroy(Node* node);
    int __cdecl GetANSI(Node* node, char* out, unsigned int size) const;
    void __cdecl Lock();
    void __cdecl Unlock();
    Node* __cdecl GetUnique(Node* node, unsigned int length);  // a private copy of node (out of the cache)
    Node* __cdecl Insert(Node* node);                          // back in, merged with an equal one

    void* unknown0;  // +0
    int liveRefs;    // +4
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

    void __cdecl Setf(const char* fmt, ...);
    int __cdecl Addf(const char* fmt, ...);
    int __cdecl Compare(const char* text) const;
    bool __cdecl Read(BinReader& r);
    bool __cdecl Write(BinWriter& w) const;

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
};

JET_LAYOUT32(sizeof(PString) == 4);
JET_LAYOUT32(offsetof(PStringCache::Node, refs) == 0x10 && offsetof(PStringCache::Node, text) == 0x18);

}  // namespace Jet
