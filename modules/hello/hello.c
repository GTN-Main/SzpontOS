/*
 * SzpontOS - Hello Kernel Module (hello.sko)
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <kernel/module.h>
#include <kernel/kprint.h>

MODULE_NAME("hello");
MODULE_AUTHOR("Szpont Industries");
MODULE_DESCRIPTION("Demonstrative Hello World Kernel Module for SzpontOS");
MODULE_LICENSE("GPL/MIT");
MODULE_VERSION("1.0.0");

static int __init_hello(void) {
    kprintf("\n\033[1;32m[hello.sko]\033[0m Hello from dynamic Szpont Kernel Object (.sko)!\n");
    kprintf("\033[1;32m[hello.sko]\033[0m Module loaded dynamically into Higher-Half Ring 0 memory.\n\n");
    return 0;
}

static void __cleanup_hello(void) {
    kprintf("\n\033[1;33m[hello.sko]\033[0m Goodbye from Szpont Kernel Object (.sko)! Cleaned up.\n\n");
}

module_init(__init_hello);
module_exit(__cleanup_hello);
