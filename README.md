# a syscall (system call) system implementation for userspace

# IMPORTANT: cross-interop

you should only use file descriptors which you own

file descriptors that come from other `SYSCALL_BASE` objects may or may not be valid

for example, `SYS_1` can have `5 fd's allocated`, `SYS_2` can have `30 fd's allocated`

care should be taken not to pass fd's from `SYS_1` to `SYS_2` (and vice versa) unless they are arguments or explicitly designed to support interop

```cpp
//   create a syscall that accepts an int argument
//
// CREATE_SYSCALL1(SYS_X, void, int)
//
```
```cpp
call_SYS_X(

	// int fd
	//
	my_fd,

	// int arg1
	//
	// it is safe to pass an fd here
	//  as long as the fd is not deallocated
	//   before the syscall returns
	//
	fd_from_another_syscall__base

)
```
```cpp
call_SYS_X(

	// int fd
	//
	// UB, the passed fd was not allocated by us and will invoke undefined behaviour
	//  it will likely crash
	//
	fd_from_another_syscall__base,

	// int arg1
	my_fd

)
```

# API

inside the `SYSCALL_BASE` object you will find important API's for implementing system calls

see `usage` for more details on some of these functions

### call_*
these functions are generated via the `CREATE_SYSCALLx` macro (where `x` is `0` through `6`)

### x__*__id
these unique identifiers are generated via the `CREATE_SYSCALLx` macro (where `x` is `0` through `6`)

these are used as an index into the generated syscall table and should be used to assign your syscall implementations

### TMP_*
these function pointers are generated via the `CREATE_SYSCALLx` macro (where `x` is `0` through `6`)

since `void*` provides no compiler feedback of incorrect function pointer assignments (`intentional, void* can be anything`)

we must use function pointers to verify a correct function pointer assignment

the `TMP_*` function pointers should be used to validate a syscall api before assigning it to the syscall table

for example, a `(void*, int, int)` should not be assigned to a syscall expecting to be called as `(void*, int, std::string)`

### create_provider_entry
this allocates a syscall provider structure to register a resource type with a set of syscalls

this can be called multiple times to register multiple structures to different syscalls

the returned structure should be passed to `allocate_fd` to create a file descriptor that uses the syscalls registered by the structure

### allocate_fd
this allocates and returns a file descriptor

### deallocate_fd
this deallocates a file descriptor

attempting to deallocate an already deallocated file descriptor is a no-op

# usage

include `libsyscall.h` anywhere in your program

it is recommended to include it inside a cpp file

```cpp
#include <libsyscall.h>
```

next, create a `struct` that extends from `SYSCALL_BASE`

```cpp
struct MY_SYSCALLS : SYSCALL_BASE {};
```

next, inside that `struct`, define system calls via `CREATE_SYSCALLx` macros, from `CREATE_SYSCALL0` to `CREATE_SYSCALL6`, with `x` being the number of arguments that system call accepts

for example

```cpp
struct MY_SYSCALLS : SYSCALL_BASE {
	CREATE_SYSCALL0(SYS_OPEN, int);
};
```

next, define a static instance of your system calls

```cpp
static MY_SYSCALLS SYS;
```

next, define a static syscall registration object to register your resource type

for the moment we only provide a single type

to support multiple types, you should register multiple providers

```cpp
static SYSCALL_BASE::SyscallProvider* register_syscalls();

static SYSCALL_BASE::SyscallProvider* provider = register_syscalls();
```

next, register your syscall provider with the syscall system

```cpp
static SYSCALL_BASE::SyscallProvider * register_syscalls() {
	puts("register syscall provider");

	// obtain a new syscall provider
	auto & res = SYS.create_provider_entry();

	// register the open syscall, this gets passed the fd, and the resource the fd holds
	SYS.TMP__SYS_OPEN = +[](int, void*) {
		puts("open from SYS_OPEN");
		return SYS.allocate_fd(*provider, nullptr, +[](size_t, void**, bool) { puts("destoy"); });
	};

	# map the syscall
	res.syscalls[SYS.x__SYS_OPEN__id] = (void*)SYS.TMP__SYS_OPEN;

	# return the provider object
	return &res;
}
```

using
```cpp
static SYSCALL_BASE::SyscallProvider* provider = register_syscalls();
```

ensures the provider will get registered when your library is opened and initialized, before any public function calls can be ran

for example, a syscall provider will be registered before `main` runs

keep in mind any providers must be registers `AFTER` the `SYSCALL_BASE` object has been initialized, otherwise you will crash

```cpp
static SYSCALL_BASE::SyscallProvider* provider = register_syscalls();

static MY_SYSCALLS SYS;

// the above will crash due to 'provider' initializing before 'SYS'
//   and 'register_syscalls' attempts to use objects that depend on 'SYS' being initialized
```

to get around this, you could make `SYS` a singleton object

```cpp
struct SYSCALL_SINGLETON {
	inline MY_SYSCALLS& instance() {
		static MY_SYSCALLS SYS;
		return SYS;
	}
};

static SYSCALL_SINGLETON SYS;
```

then update your code to call `SYS.instance().` instead of `SYS.`

this will ensure `SYS` is initialized the first time it gets used

next, we allocate a resource

```cpp
static int create_resource() {
	return SYS.instance().allocate_fd(

		// your provider
		//
		*provider,

		// an optional resource
		//
		nullptr,

		// an optional destructor for your resource, arguments are 'int fd, void** data, bool in_destructor'
		//
		// 'fd' is the fd being destroyed, the same one we obtained from 'allocate_fd'
		//
		// note that during this destructor callback, 'fd' is still a valid object
		//   and as such, we can, we can make syscalls on that 'fd' if needed
		//
		// once we leave the destructor, 'fd' will become invalid and possibly recycled by the syscall system
		//
		// this gives syscall implementations a final chance to clean up any resources still in use by an fd that may require syscalls
		//   for example, we could implement an io fd, then flush it to disk upon destruction via a flush syscall
		//    NOTE: if perf is a high factor, directly flush the '*data' object instead of going through a syscall
		//     even though syscall's are very cheap it still incurs a tiny amount of overhead
		//
		// 
		//
		// 'data' is a pointer to your passed in resource
		//
		// in this case, '*data == nullptr'
		//
		//  for example, if we pass '0x102' as the resource, it will be passed as the 'data' argument as a pointer to '0x102'
		// allocate_fd(*provider, (void*)0x102, +[](int, void** data, bool) { assert(*data == (void*)0x102); });
		//
		// 'in_destructor' is set to 'true' if we are being called from the SYSCALL_BASE destructor
		//
		// for now we simply output a message to let the user know our resource is being destroyed
		//
		+[](int, void**, bool) { puts("destroy"); }
	);
}

static void destroy_resource(int fd) {
	SYS.instance().deallocate_fd(*provider, fd);
}
```

note that any resources still alive at the end of SYS's lifetime will automatically be deallocated (and `in_destructor` will be `true`)

next, we can start to use our syscalls, we have defined an extremely limited set so we have little use

```cpp

    // create a resource
	//
	int res = create_resource();

	// invoke a syscall on that resource,
	// will throw an exception if the passed resource does not support the given syscall'
	//
	int fd = SYS.instance().call_SYS_OPEN(res);
	
	// note that in our SYS_OPEN implementation we allocate an additional fd, but we give it no arguments
```

it is worth noting that the above naming is actually incorrect

in posix for example, the `open` syscall is equivalent to our `create_resource`

now lets expend this by adding a `SYS_CLOSE` syscall

```cpp
struct MY_SYSCALLS : SYSCALL_BASE {
	// ...
	CREATE_SYSCALL0(SYS_CLOSE, void);
};

// ...

static SYSCALL_BASE::SyscallProvider * register_syscalls() {
	// ...

	// register the close syscall
	SYS.instance().TMP__SYS_CLOSE = +[](int fd, void*) {
		puts("close from SYS_CLOSE");
		return SYS.instance().deallocate_fd(*provider, fd);
	};
	res.syscalls[SYS.instance().x__SYS_CLOSE__id] = (void*)SYS.instance().TMP__SYS_CLOSE;

	return &res
}
```

you may notice that `SYS_CLOSE` is equivalent to `destroy_resource`, thats because it is

# a more-posix example

lets rewrite the above to work in a more posix-like way

```cpp
struct MY_SYSCALLS : SYSCALL_BASE {
	CREATE_SYSCALL0(SYS_READ, int);
	CREATE_SYSCALL0(SYS_WRITE, void);
};
```
```cpp
	SYS.instance().TMP__SYS_READ = +[](int fd, void*) {
		printf("read from fd %d\n", fd);
		return 0;
	};
	res.syscalls[SYS.instance().x__SYS_READ__id] = (void*)SYS.instance().TMP__SYS_READ;

	// register the close syscall
	SYS.instance().TMP__SYS_WRITE = +[](int fd, void*) {
		printf("write to fd %d\n", fd);
	};
	res.syscalls[SYS.instance().x__SYS_WRITE__id] = (void*)SYS.instance().TMP__SYS_WRITE;
```
```cpp
static inline int sys_open() {
	int fd = SYS.instance().allocate_fd(*provider, nullptr, +[](int fd, void**, bool) { SYS.instance().call_SYS_WRITE(fd); printf("closed fd %d\n", fd); });
	printf("opened fd %d\n", fd);
	return fd;
}

static inline int sys_read(int fd) {
	return SYS.instance().call_SYS_READ(fd);
}

static inline void sys_write(int fd) {
	SYS.instance().call_SYS_WRITE(fd);
}

static inline void sys_close(int fd) {
	SYS.instance().deallocate_fd(*provider, fd);
}
```

the above aligns to posix much better, in that the `sys_open` and `sys_close` are not actual syscalls but instead wrappers around fd creation/destruction

however the actual linux `open(2)` and `close(2)` syscalls are infact actual syscalls since they need to transition to kernel mode in order to modify the `file descriptor table`

here however, since we have direct access to the `userspace file descriptor table`, implementing `open` and `close` as syscall's would be pointless and require supporting true 0 argument syscalls (a syscall that takes zero arguments, `+[]() { /* a zero arg syscall */ }`)

also note that we wrap the `call_SYS_` functions in much simpler `sys_` functions that hide our internal `SYS` object from the user

now lets use the above syscalls

```cpp
	int fd1 = sys_open();
	int fd2 = sys_open();
	sys_read(fd2);
	sys_read(fd1);
	sys_write(fd1);
	sys_write(fd2);
	sys_close(fd2);
	sys_close(fd1);
```
this should result in
```
opened fd 0
opened fd 1
read from fd 1
read from fd 0
write to fd 0
write to fd 1
write to fd 1
closed fd 1
write to fd 0
closed fd 0
```
as you can see, the order in which syscalls are invoked are not important

except for 'malloc' based rules (treat file descriptor's as-if they where allocated pointers)
