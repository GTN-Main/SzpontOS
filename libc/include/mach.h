#ifndef _MACH_H
#define _MACH_H

#include <sys/types.h>

typedef uintptr_t vm_address_t;
typedef size_t vm_size_t;
typedef int vm_prot_t;
typedef int mach_port_t;

#define VM_PROT_READ 1
#define VM_PROT_WRITE 2
#define VM_PROT_EXECUTE 4
#define VM_PROT_ALL (VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE)
#define VM_INHERIT_SHARE 0

static inline mach_port_t mach_task_self(void) { return 0; }
static inline void mach_port_deallocate(mach_port_t task, mach_port_t port) { (void)task; (void)port; }

#endif /* _MACH_H */
