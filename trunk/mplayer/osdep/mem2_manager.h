#ifndef _MEM2MANAGER_H_
#define _MEM2MANAGER_H_

#include <ogc/lwp_heap.h>

#ifdef __cplusplus
extern "C" {
#endif

u32 InitMem2Manager();
void *mem2_malloc(u32 size);
void *mem2_align(u8 align, u32 size, heap_cntrl *heap_ctrl);
bool mem2_free(void *pointer, bool aligned, heap_cntrl *heap_ctrl);

#ifdef __cplusplus
}
#endif

#endif
