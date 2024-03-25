#include <libsyscall/libsyscall.h>
#include <iostream>

struct MY_SYSCALLS : SYSCALL_BASE {
	CREATE_SYSCALL0(SYS_READ, int);
	CREATE_SYSCALL0(SYS_WRITE, void);
};

static SYSCALL_BASE::SyscallProvider* register_syscalls();


static SYSCALL_BASE::SyscallProvider* provider = register_syscalls();

struct SYSCALL_SINGLETON {
	inline MY_SYSCALLS& instance() {
		static MY_SYSCALLS SYS;
		return SYS;
	}
};

static SYSCALL_SINGLETON SYS;

static SYSCALL_BASE::SyscallProvider * register_syscalls() {
	puts("register syscall provider");

	auto & res = SYS.instance().create_provider_entry();

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

	return &res;
}

static int sys_open() {
	int fd = SYS.instance().allocate_fd(*provider, nullptr, +[](int fd, void**, bool) { SYS.instance().call_SYS_WRITE(fd); printf("closed fd %d\n", fd); });
	printf("opened fd %d\n", fd);
	return fd;
}

static int sys_read(int fd) {
	return SYS.instance().call_SYS_READ(fd);
}

static void sys_write(int fd) {
	SYS.instance().call_SYS_WRITE(fd);
}

static void sys_close(int fd) {
	SYS.instance().deallocate_fd(*provider, fd);
}

int main()
{
	std::cout << "Hello CMake." << std::endl;
	int fd1 = sys_open();
	int fd2 = sys_open();
	sys_read(fd2);
	sys_read(fd1);
	sys_write(fd1);
	sys_write(fd2);
	sys_close(fd2);
	sys_close(fd1);
	return 0;
}
