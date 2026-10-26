// The "#StringData" PackedHeapMP (0x100adc18) that AnsiString and PStringCache share (AnsiString.cpp, unit U14b).
// Each frees into it by its own size rule.
#pragma once

#include <jet/KernelStandard.h>

namespace Jet::StringHeap {

// 0x10039780: the size rounded up to 8; up to 0xa0 bytes from the heap (made on first use), larger from Alloc
void* New(unsigned int size);
PackedHeapMP* Heap();

}  // namespace Jet::StringHeap
