#include "memory.h"
#include <string.h>

unsigned int GetNumBytesToAlign(unsigned int offset, unsigned int alignment) {
    unsigned int m = offset % alignment;
    return (m > 0) ? (alignment - m) : 0;
}

unsigned int GetAligned(unsigned int offset, unsigned int alignment) {
    return offset + GetNumBytesToAlign(offset, alignment);
}

void FillMemory(void *dst, int val, size_t size) {
    memset(dst, val, size);
}

void ZeroMemory(void *dst, size_t size) {
    FillMemory(dst, 0, size);
}

void CopyMemory(void *dst, void const *src, size_t size) {
    memcpy(dst, src, size);
}
