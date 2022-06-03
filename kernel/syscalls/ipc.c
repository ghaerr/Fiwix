/*
 * fiwix/kernel/syscalls/ipc.c
 *
 * Copyright 2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/config.h>
#include <fiwix/types.h>
#include <fiwix/errno.h>
#include <fiwix/process.h>
#include <fiwix/sleep.h>
#include <fiwix/string.h>
#include <fiwix/ipc.h>
#include <fiwix/msg.h>

#ifdef __DEBUG__
#include <fiwix/stdio.h>
#include <fiwix/process.h>
#endif /*__DEBUG__ */

#ifdef CONFIG_IPC
struct resource ipcmsg_resource = { 0, 0 };

void ipc_init(void)
{
	msg_init();
}

int ipc_has_perms(struct ipc_perm *perm, int mode)
{
	if(IS_SUPERUSER) {
		return 1;
	}

	if(current->euid != perm->uid || current->euid != perm->cuid) {
		mode >>= 3;
		if(current->egid != perm->gid || current->egid != perm->cgid) {
			mode >>= 3;
		}
	}

	/*
	 * The user may specify 0 for the second argument in msgget() to bypass
	 * this check and be able to obtain an identifier for a message queue,
	 * even when the user don't has read or write access to the message
	 * queue. Later msgrcv() or msgsnd() will return error if the program
	 * attempted an operation requiring read or write permission on the IPC
	 * object.
	 */
	if(!mode) {
		return 1;
	}

	if(perm->mode & mode) {
		return 1;
	}

	return 0;
}

#ifdef CONFIG_SYSCALL_6TH_ARG
int sys_ipc(unsigned int call, int first, int second, int third, void *ptr, long fifth)
{
	struct sysipc_args orig_args, *args;
	struct ipc_kludge {
		struct msgbuf *msgp;
		long msgtyp;
	} tmp;
	int version, errno;

	orig_args.arg1 = first;
	orig_args.arg2 = second;
	orig_args.arg3 = third;
	orig_args.ptr = ptr;
	orig_args.arg5 = fifth;

	if(call == MSGRCV) {
		version = call >> 16;
		switch(version) {
			case 0:
				if(!ptr) {
					return -EINVAL;
				}
				if((errno = check_user_area(VERIFY_READ, ptr, sizeof(tmp)))) {
					return errno;
				}
				memcpy_b(&tmp, (struct ipc_kludge *)ptr, sizeof(tmp));
				orig_args.arg3 = tmp.msgtyp;
				orig_args.ptr = tmp.msgp;
				orig_args.arg5 = third;
				break;
			default:
				orig_args.arg3 = fifth;
				orig_args.arg5 = third;
				break;
		}
	}
	args = &orig_args;
#else
int sys_ipc(unsigned int call, struct sysipc_args *args)
{
	if(!args) {
		return -EINVAL;
	}

#endif /* CONFIG_SYSCALL_6TH_ARG */
#ifdef __DEBUG__
	printk("(pid %d) sys_ipc(%d, 0x%08x)\n", current->pid, call, (int)args);
#endif /*__DEBUG__ */

	switch(call) {
		case MSGSND:
			return sys_msgsnd(args->arg1, args->ptr, args->arg2, args->arg3);
		case MSGRCV:
			return sys_msgrcv(args->arg1, args->ptr, args->arg2, args->arg3, args->arg5);
		case MSGGET:
			return sys_msgget((key_t)args->arg1, args->arg2);
		case MSGCTL:
			return sys_msgctl(args->arg1, args->arg2, args->ptr);
	}

	return -EINVAL;
}
#endif