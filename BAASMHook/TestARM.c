//
//  TestClass.m
//  BAASMHook
//
//  Created by arvinnie on 2020/7/9.
//  Copyright © 2020 benarvin. All rights reserved.
//

#include "TestARM.h"
#include <stddef.h>
#include <stdint.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>

void readStack() {
    uint64_t value1;
    uint64_t value2;
    uint64_t reg10;
    uint64_t reg11;

#ifdef __aarch64__
    // 先把x10、x11寄存器的值暂存起来
    __asm volatile("str x10, [%0]\n" :: "r"(&reg10));
    __asm volatile("str x11, [%0]\n" :: "r"(&reg11));
    
    // 从栈中读取到x10、x11寄存器，然后保存到变量中
//    __asm volatile("ldp x10, x11, [sp], #16\n");// 执行完这一句后，reg10和reg11的值就变了
    __asm volatile("ldp x10, x11, [sp]\n");
    __asm volatile("sub sp, sp, 16");
    __asm volatile("str x10, [%0]\n" :: "r"(&value1));
    __asm volatile("str x11, [%0]\n" :: "r"(&value2));
    
    __asm volatile("sub sp, sp, 16");

    __asm volatile("ldr x10, [%0]\n" :: "r"(&reg10));
    __asm volatile("ldr x11, [%0]\n" :: "r"(&reg11));
#endif

}
