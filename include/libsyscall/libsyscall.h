#include <vector>
#include <utility>
#include <string>
#include <stdexcept>
#include <libsyscall/wl_fd_allocator.h>

// define this to 1 - enable
// define this to 0 - disable
#define LIBSYSCALL_THREAD_SAFE 1

#if LIBSYSCALL_THREAD_SAFE
#include <shared_mutex>
#include <atomic>
#if 0

#ifndef _LGPL_SOURCE
#define _LGPL_SOURCE
#define LIBSYSCALL_LGPL_SOURCE
#endif
#ifndef URCU_INLINE_SMALL_FUNCTIONS
#define URCU_INLINE_SMALL_FUNCTIONS
#define LIBSYSCALL_URCU_INLINE_SMALL_FUNCTIONS
#endif

#include <urcu/urcu-bp.h>

#ifdef LIBSYSCALL_URCU_INLINE_SMALL_FUNCTIONS
#undef URCU_INLINE_SMALL_FUNCTIONS
#undef LIBSYSCALL_URCU_INLINE_SMALL_FUNCTIONS
#endif

#ifdef LIBSYSCALL_LGPL_SOURCE
#undef _LGPL_SOURCE
#undef LIBSYSCALL_LGPL_SOURCE
#endif

struct libsyscall__recursive_shared_mutex {
	std::atomic_int32_t locked = { 0 };
	std::recursive_mutex m;
	std::shared_mutex mutex;
	
	inline void lock() {
		locked.load() == 0 ? (void)mutex.lock() : (void)locked++;
	}
};

#define LIBSYSCALL__MUTEX_VARIABLE std::shared_mutex mutex; std::atomic_bool locked = false;
#define LIBSYSCALL__MUTEX_GUARD_VARIABLE std::lock_guard<libsyscall_recursive_shared_mutex> guard(mutex);
#else
#define LIBSYSCALL__MUTEX_VARIABLE std::shared_mutex mutex; std::atomic_bool locked = false;
#define LIBSYSCALL__MUTEX_GUARD_VARIABLE std::lock_guard<std::shared_mutex> guard(mutex);
#endif
#else
#define LIBSYSCALL__MUTEX_VARIABLE
#define LIBSYSCALL__MUTEX_GUARD_VARIABLE
#endif

#define CREATE_SYSCALL0(name, ret) \
ret (*TMP__##name) (int, void *); \
int x__##name##__id = +[this]() { syscalls.push_back(nullptr); return syscalls.size()-1; }(); \
ret call_##name(int fd) { \
	Resource& res = wl_miniobj_get_priv(fd); \
	void * callback = res.syscalls[0][x__##name##__id]; \
    if (callback != nullptr) return ((ret (*) (int, void *))callback)(fd, res.resource); \
	throw new std::runtime_error("callback not supported"); \
}

#define CREATE_SYSCALL1(name, ret, p1) \
ret (*TMP__##name) (int, void*, p1); \
int x__##name##__id = +[this]() { syscalls.push_back(nullptr); return syscalls.size()-1; }(); \
ret call_##name(int fd, p1 arg1) { \
	Resource& res = wl_miniobj_get_priv(fd); \
	void * callback = res.syscalls[0][x__##name##__id]; \
    if (callback != nullptr) return ((ret (*) (int, void*, p1))callback)(fd, res.resource, arg1); \
	throw new std::runtime_error("callback not supported"); \
}

#define CREATE_SYSCALL2(name, ret, p1, p2) \
ret (*TMP__##name) (int, void*, p1, p2); \
int x__##name##__id = +[this]() { syscalls.push_back(nullptr); return syscalls.size()-1; }(); \
ret call_##name(int fd, p1 arg1, p2 arg2) { \
	Resource& res = wl_miniobj_get_priv(fd); \
	void * callback = res.syscalls[0][x__##name##__id]; \
    if (callback != nullptr) return ((ret (*) (int, void*, p1, p2))callback)(fd, res.resource, arg1, arg2); \
	throw new std::runtime_error("callback not supported"); \
}

#define CREATE_SYSCALL3(name, ret, p1, p2, p3) \
ret (*TMP__##name) (int, void*, p1, p2, p3); \
int x__##name##__id = +[this]() { syscalls.push_back(nullptr); return syscalls.size()-1; }(); \
ret call_##name(int fd, p1 arg1, p2 arg2, p3 arg3) { \
	Resource& res = wl_miniobj_get_priv(fd); \
	void * callback = res.syscalls[0][x__##name##__id]; \
    if (callback != nullptr) return ((ret (*) (int, void*, p1, p2, p3))callback)(fd, res.resource, arg1, arg2, arg3); \
	throw new std::runtime_error("callback not supported"); \
}

#define CREATE_SYSCALL4(name, ret, p1, p2, p3, p4) \
ret (*TMP__##name) (int, void*, p1, p2, p3, p4); \
int x__##name##__id = +[this]() { syscalls.push_back(nullptr); return syscalls.size()-1; }(); \
ret call_##name(int fd, p1 arg1, p2 arg2, p3 arg3, p4 arg4) { \
	Resource& res = wl_miniobj_get_priv(fd); \
	void * callback = res.syscalls[0][x__##name##__id]; \
    if (callback != nullptr) return ((ret (*) (int, void*, p1, p2, p3, p4))callback)(fd, res.resource, arg1, arg2, arg3, arg4); \
	throw new std::runtime_error("callback not supported"); \
}

#define CREATE_SYSCALL5(name, ret, p1, p2, p3, p4, p5) \
ret (*TMP__##name) (int, void*, p1, p2, p3, p4, p5); \
int x__##name##__id = +[this]() { syscalls.push_back(nullptr); return syscalls.size()-1; }(); \
ret call_##name(int fd, p1 arg1, p2 arg2, p3 arg3, p4 arg4, p5 arg5) { \
	Resource& res = wl_miniobj_get_priv(fd); \
	void * callback = res.syscalls[0][x__##name##__id]; \
    if (callback != nullptr) return ((ret (*) (int, void*, p1, p2, p3, p4, p5))callback)(fd, res.resource, arg1, arg2, arg3, arg4, arg5); \
	throw new std::runtime_error("callback not supported"); \
}

#define CREATE_SYSCALL6(name, ret, p1, p2, p3, p4, p5, p6) \
ret (*TMP__##name) (int, void*, p1, p2, p3, p4, p5, p6); \
int x__##name##__id = +[this]() { syscalls.push_back(nullptr); return syscalls.size()-1; }(); \
ret call_##name(int fd, p1 arg1, p2 arg2, p3 arg3, p4 arg4, p5 arg5, p6 arg6) { \
	Resource& res = wl_miniobj_get_priv(fd); \
	void * callback = res.syscalls[0][x__##name##__id]; \
    if (callback != nullptr) return ((ret (*) (int, void*, p1, p2, p3, p4, p5, p6))callback)(fd, res.resource, arg1, arg2, arg3, arg4, arg5, arg6); \
	throw new std::runtime_error("callback not supported"); \
}

struct SYSCALL_BASE {
public:
	struct SyscallProvider {
		std::vector<void*> syscalls;
		inline SyscallProvider() {}
		inline SyscallProvider(std::vector<void*> syscalls) : syscalls(syscalls) {}
	};
	struct Resource {
		std::vector<void*> * syscalls;
		void* resource = nullptr;
		inline Resource() {}
		inline Resource(SyscallProvider & provider, void * resource) : syscalls(&provider.syscalls), resource(resource) {}
	};
	LIBSYSCALL__MUTEX_VARIABLE
protected:
	std::vector<SyscallProvider> provider_table;
	std::vector<void*> syscalls;
	wl_syscalls__fd_allocator* descriptor_list;

	Resource& wl_miniobj_get_priv(int fd) {
		LIBSYSCALL__MUTEX_GUARD_VARIABLE
		if (fd == -1) {
			throw new std::runtime_error("SYSCALL_BASE ERROR: fd is -1");
		}
		if (!wl_syscalls__fd_allocator__fd_is_valid(descriptor_list, fd)) {
			std::string msg = "SYSCALL_BASE ERROR: fd (" + std::to_string(fd) + ") is invalid";
			throw new std::runtime_error(msg.c_str());
		}
		return *(Resource*)wl_syscalls__fd_allocator__get_value_from_fd(descriptor_list, fd);
	}

public:
	inline SyscallProvider& create_provider_entry() {
		LIBSYSCALL__MUTEX_GUARD_VARIABLE
		provider_table.emplace_back(SyscallProvider(syscalls));
		return provider_table.back();
	}

	inline int allocate_fd(SyscallProvider & provider, void * resource, WL_SYSCALLS_FD_ALLOCATOR_CALLBACK___DESTROY_DATA destroy_callback) {
		LIBSYSCALL__MUTEX_GUARD_VARIABLE
		return wl_syscalls__fd_allocator__allocate_fd(descriptor_list, new Resource(provider, resource), destroy_callback);
	}

	inline void deallocate_fd(SyscallProvider& provider, int fd) {
		LIBSYSCALL__MUTEX_GUARD_VARIABLE
		wl_syscalls__fd_allocator__deallocate_fd(descriptor_list, fd);
	}

	inline SYSCALL_BASE() {
		descriptor_list = wl_syscalls__fd_allocator__create();
		if (descriptor_list == NULL)
			throw new std::runtime_error("SYSCALL_BASE() ERROR:       FAILED TO INITIALIZE FD ALLOCATOR");
	}

	inline virtual ~SYSCALL_BASE() {
		size_t objcount = wl_syscalls__fd_allocator__size(descriptor_list);
		if (objcount != 0) {
			printf("~SYSCALL_BASE() WARNING: there are %zu allocated objects still present, they will be destroyed\n", objcount);
		}
		wl_syscalls__fd_allocator__destroy(descriptor_list);
	}
};
