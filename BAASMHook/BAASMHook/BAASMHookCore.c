//
//  BAASMHookCore.c
//  BAASMHook
//
//  Created by BenArvin on 2020/07/14.
//  Copyright © 2017年 BenArvin. All rights reserved.
//

#include "BAASMHookCore.h"
#include <stdio.h>

#ifdef __aarch64__

#import "fishhook.h"
#include <stdlib.h>
#include <string.h>
#include <dispatch/dispatch.h>
#include <pthread.h>
#include <objc/message.h>

#pragma mark - defines

static void(*_onBeforeCall)(Class, SEL);
static void(*_onAfterCall)(Class, SEL);

static pthread_mutex_t _hooking_lock;
static BOOL _hooking = true;

static pthread_key_t _thread_key;
__unused static id (*orig_objc_msgSend)(id, SEL, ...);

typedef struct {
    id self; //通过 object_getClass 能够得到 Class 再通过 NSStringFromClass 能够得到类名
    Class cls;
    SEL cmd; //通过 NSStringFromSelector 方法能够得到方法名
    uintptr_t lr; // link register
    uint64_t reg_0;
    uint64_t reg_0_rt;
    uint64_t reg_1;
    uint64_t reg_1_rt;
    uint64_t reg_2;
    uint64_t reg_3;
    uint64_t reg_4;
    uint64_t reg_5;
    uint64_t reg_6;
    uint64_t reg_7;
    uint64_t reg_8;
    uint64_t reg_9;
} thread_call_record;

typedef struct {
    thread_call_record *stack;
    int allocated_length;
    int index;
} thread_call_stack;

#pragma mark - pthread stack methods
static inline thread_call_stack * get_thread_call_stack() {
    thread_call_stack *cs = (thread_call_stack *)pthread_getspecific(_thread_key);
    if (cs == NULL) {
        cs = (thread_call_stack *)malloc(sizeof(thread_call_stack));
        cs->stack = (thread_call_record *)calloc(128, sizeof(thread_call_record));
        cs->allocated_length = 64;
        cs->index = -1;
        pthread_setspecific(_thread_key, cs);
    }
    return cs;
}

static void release_thread_call_stack(void *ptr) {
    thread_call_stack *cs = (thread_call_stack *)ptr;
    if (!cs) return;
    if (cs->stack) free(cs->stack);
    free(cs);
}

#pragma mark - flag methods
bool is_hooking(void) {
    bool result = true;
    pthread_mutex_lock(&_hooking_lock);
    result = _hooking;
    pthread_mutex_unlock(&_hooking_lock);
    return result;
}

void set_hooking(bool newValue) {
    pthread_mutex_lock(&_hooking_lock);
    _hooking = newValue;
    pthread_mutex_unlock(&_hooking_lock);
}

#pragma mark - business methods
void before_call(thread_call_record *record) {
    if (!is_hooking()) {
        return;
    }
    const char *className = class_getName(record->cls);
    if (strcmp(className, "BAASMHook") == 0) {
        return;
    }
//    const char *cmdName = sel_getName(record->cmd);
//    if (strcmp(cmdName, "testParamFunc:") == 0) {
//        record->reg_2 = 987654321;
//    } else if (strcmp(cmdName, "add:num2:") == 0) {
//        record->reg_2 = 3;
//    }
//    if (_onBeforeCall) {
//        _onBeforeCall(record->cls, record->cmd);
//    }
}

void after_call(thread_call_record *record) {
    if (!is_hooking()) {
        return;
    }
    const char *className = class_getName(record->cls);
    if (strcmp(className, "BAASMHook") == 0) {
        return;
    }
    const char *cmdName = sel_getName(record->cmd);
//    if (strcmp(cmdName, "add:num2:") == 0) {
//        record->reg_0_rt = 4;
//    }
    if (_onAfterCall) {
        _onAfterCall(record->cls, record->cmd);
    }
    
}

static inline void push_call_record(id _self, Class _cls, SEL _cmd, uintptr_t lr, uint64_t reg_sp, int *index) {
    thread_call_stack *cs = get_thread_call_stack();
    if (cs) {
        int nextIndex = (++cs->index);
        if (nextIndex >= cs->allocated_length) {
            cs->allocated_length += 64;
            cs->stack = (thread_call_record *)realloc(cs->stack, cs->allocated_length * sizeof(thread_call_record));
        }
        *index = nextIndex;
        thread_call_record *newRecord = &cs->stack[nextIndex];
        newRecord->self = _self;
        newRecord->cls = _cls;
        newRecord->cmd = _cmd;
        newRecord->lr = lr;
        
        uint64_t reg_tmp_19;
        uint64_t reg_tmp_20;

        __asm volatile("str x19, [%0]\n" :: "r"(&reg_tmp_19));
        __asm volatile("str x20, [%0]\n" :: "r"(&reg_tmp_20));

        __asm volatile("ldp x19, x20, [sp, 192]\n");
        __asm volatile("str x19, [%0]\n" :: "r"(&newRecord->reg_0));
        __asm volatile("str x20, [%0]\n" :: "r"(&newRecord->reg_1));
        __asm volatile("ldp x19, x20, [sp, 208]\n");
        __asm volatile("str x19, [%0]\n" :: "r"(&newRecord->reg_2));
        __asm volatile("str x20, [%0]\n" :: "r"(&newRecord->reg_3));
        __asm volatile("ldp x19, x20, [sp, 224]\n");
        __asm volatile("str x19, [%0]\n" :: "r"(&newRecord->reg_4));
        __asm volatile("str x20, [%0]\n" :: "r"(&newRecord->reg_5));
        __asm volatile("ldp x19, x20, [sp, 240]\n");
        __asm volatile("str x19, [%0]\n" :: "r"(&newRecord->reg_6));
        __asm volatile("str x20, [%0]\n" :: "r"(&newRecord->reg_7));
        __asm volatile("ldp x19, x20, [sp, 256]\n");
        __asm volatile("str x19, [%0]\n" :: "r"(&newRecord->reg_8));
        __asm volatile("str x20, [%0]\n" :: "r"(&newRecord->reg_9));

        __asm volatile("ldr x19, [%0]\n" :: "r"(&reg_tmp_19));
        __asm volatile("ldr x20, [%0]\n" :: "r"(&reg_tmp_20));
    }
}

void reset_params(SEL _cmd, int index, uint64_t reg_sp) {
    thread_call_stack *cs = get_thread_call_stack();
    thread_call_record *record = &cs->stack[index];
    
    before_call(record);
    
    uint64_t reg_tmp_19;
    uint64_t reg_tmp_20;
    __asm volatile("str x19, [%0]\n" :: "r"(&reg_tmp_19));
    __asm volatile("str x20, [%0]\n" :: "r"(&reg_tmp_20));

    __asm volatile("ldr x19, [%0]\n" :: "r"(&record->reg_0));
    __asm volatile("ldr x20, [%0]\n" :: "r"(&record->reg_1));
    __asm volatile("stp x19, x20, [sp, 160]\n");
    
    __asm volatile("ldr x19, [%0]\n" :: "r"(&record->reg_2));
    __asm volatile("ldr x20, [%0]\n" :: "r"(&record->reg_3));
    __asm volatile("stp x19, x20, [sp, 176]\n");
    
    __asm volatile("ldr x19, [%0]\n" :: "r"(&record->reg_4));
    __asm volatile("ldr x20, [%0]\n" :: "r"(&record->reg_5));
    __asm volatile("stp x19, x20, [sp, 192]\n");
    
    __asm volatile("ldr x19, [%0]\n" :: "r"(&record->reg_6));
    __asm volatile("ldr x20, [%0]\n" :: "r"(&record->reg_7));
    __asm volatile("stp x19, x20, [sp, 208]\n");
    
    __asm volatile("ldr x19, [%0]\n" :: "r"(&record->reg_8));
    __asm volatile("ldr x20, [%0]\n" :: "r"(&record->reg_9));
    __asm volatile("stp x19, x20, [sp, 224]\n");

    __asm volatile("ldr x19, [%0]\n" :: "r"(&reg_tmp_19));
    __asm volatile("ldr x20, [%0]\n" :: "r"(&reg_tmp_20));
}

void before_objc_msgSend(id self, SEL _cmd, uintptr_t lr) {
    uint64_t reg_sp;
    uint64_t reg19;
    __asm volatile("str x19, [%0]\n" :: "r"(&reg19));
    __asm volatile("mov x19, sp");
    __asm volatile("str x19, [%0]\n" :: "r"(&reg_sp));
    __asm volatile("ldr x19, [%0]\n" :: "r"(&reg19));
    
    //sp相差160，除去主动入栈的10个寄存器，剩下的就是函数调用时入栈的
    //PC、LR、SP、FP，加上自身参数self、_cmd、lr，内部变量reg_sp、reg19、index，共10个寄存器
    reg_sp = reg_sp + 80;
    
    int index;
    push_call_record(self, object_getClass(self), _cmd, lr, reg_sp, &index);
    reset_params(_cmd, index, reg_sp);
}

uintptr_t after_objc_msgSend(void) {
    //获取之前保存的x0和x1寄存器的值
    uint64_t reg_tmp_0;
    uint64_t reg_tmp_1;
    uint64_t reg_tmp_19;
    uint64_t reg_tmp_20;
    __asm volatile("str x19, [%0]\n" :: "r"(&reg_tmp_19)); //将x19寄存器的值存入reg19中
    __asm volatile("str x20, [%0]\n" :: "r"(&reg_tmp_20)); //将x20寄存器的值存入reg19中
    __asm volatile("ldp x19, x20, [sp, 96]\n"); //出栈，sp偏移为112，是因为方法跳转需要固定压栈8个寄存器即大小64，本方法内有6个局部变量即再加上大小48
    __asm volatile("str x19, [%0]\n" :: "r"(&reg_tmp_0)); //将x19寄存器值位置的数据，也就是之前保存的x0的值，读取到reg_tmp_0中
    __asm volatile("str x20, [%0]\n" :: "r"(&reg_tmp_1)); //将x20寄存器值位置的数据，也就是之前保存的x1的值，读取到reg_tmp_1中
    __asm volatile("ldr x19, [%0]\n" :: "r"(&reg_tmp_19)); //将存储器地址为reg19的字数据，读入寄存器x19，也就是恢复x19寄存器
    __asm volatile("ldr x20, [%0]\n" :: "r"(&reg_tmp_20)); //将存储器地址为reg20的字数据，读入寄存器x20，也就是恢复x20寄存器
    
    thread_call_stack *cs = get_thread_call_stack();
    int nextIndex = cs->index--;
    thread_call_record *pRecord = &cs->stack[nextIndex];
    pRecord->reg_0_rt = reg_tmp_0;
    pRecord->reg_1_rt = reg_tmp_1;

    after_call(pRecord);

    __asm volatile("str x19, [%0]\n" :: "r"(&reg_tmp_19)); //将x19寄存器的值存入reg19中
    __asm volatile("str x20, [%0]\n" :: "r"(&reg_tmp_20)); //将x20寄存器的值存入reg19中
    __asm volatile("ldr x19, [%0]\n" :: "r"(&pRecord->reg_0_rt)); //将存储器地址为reg_0_rt的字数据，读入寄存器x19
    __asm volatile("ldr x20, [%0]\n" :: "r"(&pRecord->reg_1_rt)); //将存储器地址为reg_1_rt的字数据，读入寄存器x20
    __asm volatile("stp x19, x20, [sp, 96]\n"); //入栈，即复写x0和x1
    __asm volatile("ldr x19, [%0]\n" :: "r"(&reg_tmp_19)); //将存储器地址为reg19的字数据，读入寄存器x19，也就是恢复x19寄存器
    __asm volatile("ldr x20, [%0]\n" :: "r"(&reg_tmp_20)); //将存储器地址为reg20的字数据，读入寄存器x20，也就是恢复x20寄存器
    
    return pRecord->lr;// 把lr当作返回值，放进x0寄存器
}

#pragma mark - hook methods

//replacement objc_msgSend (arm64)
// https://blog.nelhage.com/2010/10/amd64-and-va_arg/
// http://infocenter.arm.com/help/topic/com.arm.doc.ihi0055b/IHI0055B_aapcs64.pdf
// https://developer.apple.com/library/ios/documentation/Xcode/Conceptual/iPhoneOSABIReference/Articles/ARM64FunctionCallingConventions.html
// https://github.com/ming1016/GCDFetchFeed/tree/master/GCDFetchFeed/GCDFetchFeed/Lib/SMLagMonitor/SMCallTraceCore.c

// x0-x7: 函数参数，x0存储函数返回值
// x8: 间接结果寄存器，用于保存子程序返回地址
// x9-x15: 调用者临时存储用
// x30(LR): 链接寄存器，下一条执行的代码段地址
// x31(SP): 堆栈指针寄存器



// 1.存下返回值
// 2.把要调用的函数代码段地址存到x12
// 3.恢复返回值
// 4.跳转到x12寄存器指向的位置，也就是要调用的函数位置
#define call(value) \
__asm volatile ("stp x8, x9, [sp, #-16]!\n"); \
__asm volatile ("mov x12, %0\n" :: "r"(value)); \
__asm volatile ("ldp x8, x9, [sp], #16\n"); \
__asm volatile ("blr x12\n");

// 保存函数入参(x0-x8)到栈内存，因为栈指针移动必须满足SP Mod 16 = 0的条件，而在x8寄存器只占用8个字节，剩余8个字节由 x9 来填充
// !意思是执行加减操作
// 数字在[]里面还是外面，表示立即执行加减操作，或者寄存器命令执行完再进行加减
//ldr x0, [x1]; 将寄存器x1的值作为地址，取该内存地址的值放入寄存器x0中
//ldr w8, [sp, #0x8]; 将栈内存[sp + 0x8]处的值读取到w8寄存器中
//ldr x0, [x1, #4]!; 将寄存器x1的值加上4作为内存地址, 取该内存地址的值放入寄存器x0中，然后将寄存器x1的值加上4放入寄存器x1中
//ldr x0, [x1], #4; 将寄存器x1的值作为内存地址，取内该存地址的值放入寄存器x0中，再将寄存器x1的值加上4放入寄存器x1中
//ldr x0, [x1, x2]; 将寄存器x1和寄存器x2的值相加作为地址，取该内存地址的值放入寄存器x0中
#define save() \
__asm volatile ( \
"stp x8, x9, [sp, #-16]!\n" \
"stp x6, x7, [sp, #-16]!\n" \
"stp x4, x5, [sp, #-16]!\n" \
"stp x2, x3, [sp, #-16]!\n" \
"stp x0, x1, [sp, #-16]!\n");

// 把刚才保存的参数出栈到寄存器
#define load() \
__asm volatile ( \
"ldp x0, x1, [sp], #16\n" \
"ldp x2, x3, [sp], #16\n" \
"ldp x4, x5, [sp], #16\n" \
"ldp x6, x7, [sp], #16\n" \
"ldp x8, x9, [sp], #16\n" );

__attribute__((__naked__))
static void baasm_objc_msgSend() {
    // Save parameters.
    save()
    
    // 把lr寄存器（下一条执行的代码段地址）存入x2，当作before_objc_msgSend函数的第3个参数，传递过去后保存起来
    __asm volatile ("mov x2, lr\n");
//    __asm volatile ("mov x3, x4\n");
    
    // Call our before_objc_msgSend.
    call(&before_objc_msgSend)
    
    // Load parameters.
    load()
    
    // Call through to the original objc_msgSend.
    call(orig_objc_msgSend)
    
    // Save original objc_msgSend return value.
    save()
    
    // Call our after_objc_msgSend.
    call(&after_objc_msgSend)
    
    // after_objc_msgSend函数的返回值是之前存下的lr，会放到x0寄存器中
    // 这时把x0寄存器中的值放入lr寄存器，就恢复了最初保存的下一条执行的代码段地址
    __asm volatile ("mov lr, x0\n");
    
    // Load original objc_msgSend return value.
    load()
    
    // return
    __asm volatile ("ret\n");
}


#pragma mark public

void startAsmHook() {
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        pthread_mutex_init(&_hooking_lock, NULL);
        pthread_key_create(&_thread_key, &release_thread_call_stack);
        struct rebinding bds[1] = {
            {"objc_msgSend", baasm_objc_msgSend, (void *)&orig_objc_msgSend}
        };
        rebind_symbols(bds, 1);
    });
    set_hooking(true);
}

void stopAsmHook() {
    set_hooking(false);
}

void setCallback(void(*onBeforeCall)(Class, SEL), void(*onAfterCall)(Class, SEL)) {
    _onBeforeCall = onBeforeCall;
    _onAfterCall = onAfterCall;
}

#else

void startAsmHook() {
    printf("BAASMHook only support device environment!");
}

void stopAsmHook() {
    printf("BAASMHook only support device environment!");
}

void setCallback(void(*onBeforeCall)(Class, SEL), void(*onAfterCall)(Class, SEL)) {
    printf("BAASMHook only support device environment!");
}

#endif
