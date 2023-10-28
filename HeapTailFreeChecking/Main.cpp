#include <Windows.h>
#include <iostream>
#include <stdio.h>
using namespace std;

typedef struct HEAP_ENTRY {
    uint16_t Size;
    uint8_t  Flags;
    uint8_t  SmallTagIndex;
    uint16_t PreviousSize;
    uint8_t  SegmentOffset;
    uint8_t  UnusedBytes;
}HEAP_ENTRY, * PHEAP_ENTRY;

void GetDecodedHeapEntry(PHEAP_ENTRY HeapEntry, uintptr_t Granularity) {
    HeapEntry->Size = HeapEntry->Size * Granularity;
    HeapEntry->Flags = HeapEntry->Flags * Granularity;
    HeapEntry->SmallTagIndex = HeapEntry->SmallTagIndex * Granularity;
    HeapEntry->PreviousSize = HeapEntry->PreviousSize * Granularity;
    HeapEntry->SegmentOffset = HeapEntry->SegmentOffset * Granularity;
    HeapEntry->UnusedBytes = HeapEntry->UnusedBytes * Granularity;
}

BYTE* GetTailSequence(uintptr_t ProcessHeap, uintptr_t Offset, uintptr_t size) {
    BYTE* pTailSequenceStart = reinterpret_cast<BYTE*>(ProcessHeap + Offset + size - 0x24);
    int i = 0;
    for (i = 0; i != 0x30; i++) {
        if (*(pTailSequenceStart + i) == 0xab) {
            if(*(pTailSequenceStart + i + 1) == 0xab)
                break;
        }
    }
    return pTailSequenceStart + i;
}

int main() {
    BOOL isDebugged = false;
    HEAP_ENTRY HeapEntry = { 0 };
    //Get the default heap
    uintptr_t pPEB = 0;
    _asm {
        mov eax, fs: [30h]
        mov pPEB, eax
    }
    if (!pPEB) {
        cout << "Unable to get the PEB!" << endl;
        return -1;
    }
    uintptr_t ProcessHeap = *reinterpret_cast<uintptr_t*>(pPEB + 0x18);

    // Check the Encoding field
    uintptr_t Encoding1 = *reinterpret_cast<uintptr_t*>(ProcessHeap + 0x50);
    uintptr_t Encoding2 = *reinterpret_cast<uintptr_t*>(ProcessHeap + 0x54);
    DWORD     Offset = 0;
    while (true) {
        uintptr_t EncodedHeapEntry1 = *reinterpret_cast<uintptr_t*>(ProcessHeap + Offset);
        uintptr_t EncodedHeapEntry2 = *reinterpret_cast<uintptr_t*>(ProcessHeap + 0x4 + Offset);
        *reinterpret_cast<uintptr_t*>(&HeapEntry) = EncodedHeapEntry1 ^ Encoding1;
        *(reinterpret_cast<uintptr_t*>(&HeapEntry) + 1) = EncodedHeapEntry2 ^ Encoding2;
        GetDecodedHeapEntry(&HeapEntry, 0x8);                              // For 64-bit system on Windows 11 Granularity = 0x8 bytes
        if (HeapEntry.Flags == 0x8) {                                      // Block is the first or last block
            if (HeapEntry.PreviousSize != 0)                               // Block is the last block
                break;
            if (HeapEntry.PreviousSize == 0) {                             // Block is the first block
                Offset += HeapEntry.Size;
                continue;
            }
        }
        if (HeapEntry.Flags == 0x38 || HeapEntry.Flags == 0x78) {          // State is busy (allocated)
            uintptr_t* pTailSequenceStart = reinterpret_cast<uintptr_t*>(GetTailSequence(ProcessHeap, Offset, HeapEntry.Size));
            uintptr_t TailCheck1 = *pTailSequenceStart;
            uintptr_t TailCheck2 = *(pTailSequenceStart + 1);
            if (TailCheck1 == 0xabababab && TailCheck2 == 0xabababab)
                isDebugged = true;
            else {
                isDebugged = false;
                break;
            }
        }
        if (HeapEntry.Flags == 0x20) {
            if (HeapEntry.Size == 0x10) {
                Offset += HeapEntry.Size;
                continue;
            }
            uintptr_t* pFreeCheck = reinterpret_cast<uintptr_t*>(ProcessHeap + Offset + 0x10);
            uintptr_t LenFreeCheckBytes = HeapEntry.Size - 0x10;
            uintptr_t FreeCheck;
            for (int i = 0; i != LenFreeCheckBytes / 4; i++) {
                FreeCheck = *(pFreeCheck + i);
                if (FreeCheck == 0xfeeefeee)
                    isDebugged = true;
                else {
                    isDebugged = false;
                    break;
                }
            }
        }
        Offset += HeapEntry.Size;
    }

    if (isDebugged)
        cout << "The process is being debugged" << endl;
    else
        cout << "The process is not being debugged" << endl;
    return 0;
}

