#ifndef WL_SYSCALLS_FD_ALLOCATOR_H
#define WL_SYSCALLS_FD_ALLOCATOR_H

#include <stddef.h>

// set this to true to enable printf debugging
extern
#ifdef __cplusplus
"C"
#endif
bool wl_miniobj_debug;

typedef int wl_syscalls__fd_allocator__size_t;
#define wl_syscalls__fd_allocator__size_t_MAX INT_MAX

typedef struct wl_syscalls__fd_allocator {
    void* used;
    void* recycled;
} wl_syscalls__fd_allocator;

typedef void (*WL_SYSCALLS_FD_ALLOCATOR_CALLBACK___DESTROY_DATA)(int fd, void** data, bool in_destructor);

#ifdef __cplusplus
extern "C" {
#endif

    wl_syscalls__fd_allocator* wl_syscalls__fd_allocator__create(void);
    void                        wl_syscalls__fd_allocator__destroy(wl_syscalls__fd_allocator* fd);
    int                         wl_syscalls__fd_allocator__allocate_fd(
        wl_syscalls__fd_allocator* wl_syscalls__fd_allocator, void* data, WL_SYSCALLS_FD_ALLOCATOR_CALLBACK___DESTROY_DATA callback
    );
    size_t                      wl_syscalls__fd_allocator__size(wl_syscalls__fd_allocator* wl_syscalls__fd_allocator);
    size_t                      wl_syscalls__fd_allocator__capacity(wl_syscalls__fd_allocator* wl_syscalls__fd_allocator);
    bool                        wl_syscalls__fd_allocator__fd_is_valid(wl_syscalls__fd_allocator* wl_syscalls__fd_allocator, int fd);
    void* wl_syscalls__fd_allocator__get_value_from_fd(wl_syscalls__fd_allocator* wl_syscalls__fd_allocator, int fd);
    void                        wl_syscalls__fd_allocator__deallocate_fd(wl_syscalls__fd_allocator* wl_syscalls__fd_allocator, int fd);

    void* KNHeap__create(void);
    void  KNHeap__destroy(void* instance);
    int   KNHeap__getSize(void* instance);
    void  KNHeap__getMin(void* instance, wl_syscalls__fd_allocator__size_t* key, void** value);
    void  KNHeap__deleteMin(void* instance, wl_syscalls__fd_allocator__size_t* key, void** value);
    void  KNHeap__insert(void* instance, wl_syscalls__fd_allocator__size_t key, void* value);

    void* ShrinkingVectorIndexAllocator__create(void);
    void   ShrinkingVectorIndexAllocator__destroy(void* instance);
    size_t ShrinkingVectorIndexAllocator__size(void* instance);
    size_t ShrinkingVectorIndexAllocator__capacity(void* instance);
    size_t ShrinkingVectorIndexAllocator__add(void* instance, void* value, WL_SYSCALLS_FD_ALLOCATOR_CALLBACK___DESTROY_DATA callback);
    size_t ShrinkingVectorIndexAllocator__reuse(void* instance, size_t index, void* value, WL_SYSCALLS_FD_ALLOCATOR_CALLBACK___DESTROY_DATA callback);
    bool   ShrinkingVectorIndexAllocator__index_is_valid(void* instance, size_t index);
    void* ShrinkingVectorIndexAllocator__data(void* instance, size_t index);
    bool   ShrinkingVectorIndexAllocator__remove(void* instance, int index);

#ifdef __cplusplus
}
#endif

#endif // WL_SYSCALLS_FD_ALLOCATOR_H
