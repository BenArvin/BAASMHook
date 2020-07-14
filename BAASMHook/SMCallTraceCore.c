//
//  SMCallTraceCore.c
//  DecoupleDemo
//
//  Created by DaiMing on 2017/7/16.
//  Copyright © 2017年 Starming. All rights reserved.
//

#include "SMCallTraceCore.h"


#ifdef __aarch64__

#pragma mark - fishhook
#include <stddef.h>
#include <stdint.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>

/*
 * A structure representing a particular intended rebinding from a symbol
 * name to its replacement
 */
struct rebinding {
    const char *name;
    void *replacement;
    void **replaced;
};

/*
 * For each rebinding in rebindings, rebinds references to external, indirect
 * symbols with the specified name to instead point at replacement for each
 * image in the calling process as well as for all future images that are loaded
 * by the process. If rebind_functions is called more than once, the symbols to
 * rebind are added to the existing list of rebindings, and if a given symbol
 * is rebound more than once, the later rebinding will take precedence.
 */
static int fish_rebind_symbols(struct rebinding rebindings[], size_t rebindings_nel);


#ifdef __LP64__
typedef struct mach_header_64 mach_header_t;
typedef struct segment_command_64 segment_command_t;
typedef struct section_64 section_t;
typedef struct nlist_64 nlist_t;
#define LC_SEGMENT_ARCH_DEPENDENT LC_SEGMENT_64
#else
typedef struct mach_header mach_header_t;
typedef struct segment_command segment_command_t;
typedef struct section section_t;
typedef struct nlist nlist_t;
#define LC_SEGMENT_ARCH_DEPENDENT LC_SEGMENT
#endif

#ifndef SEG_DATA_CONST
#define SEG_DATA_CONST  "__DATA_CONST"
#endif

struct rebindings_entry {
    struct rebinding *rebindings;
    size_t rebindings_nel;
    struct rebindings_entry *next;
};

static struct rebindings_entry *_rebindings_head;

static int prepend_rebindings(struct rebindings_entry **rebindings_head,
                              struct rebinding rebindings[],
                              size_t nel) {
    struct rebindings_entry *new_entry = malloc(sizeof(struct rebindings_entry));
    if (!new_entry) {
        return -1;
    }
    new_entry->rebindings = malloc(sizeof(struct rebinding) * nel);
    if (!new_entry->rebindings) {
        free(new_entry);
        return -1;
    }
    memcpy(new_entry->rebindings, rebindings, sizeof(struct rebinding) * nel);
    new_entry->rebindings_nel = nel;
    new_entry->next = *rebindings_head;
    *rebindings_head = new_entry;
    return 0;
}

static void perform_rebinding_with_section(struct rebindings_entry *rebindings,
                                           section_t *section,
                                           intptr_t slide,
                                           nlist_t *symtab,
                                           char *strtab,
                                           uint32_t *indirect_symtab) {
    uint32_t *indirect_symbol_indices = indirect_symtab + section->reserved1;
    void **indirect_symbol_bindings = (void **)((uintptr_t)slide + section->addr);
    for (uint i = 0; i < section->size / sizeof(void *); i++) {
        uint32_t symtab_index = indirect_symbol_indices[i];
        if (symtab_index == INDIRECT_SYMBOL_ABS || symtab_index == INDIRECT_SYMBOL_LOCAL ||
            symtab_index == (INDIRECT_SYMBOL_LOCAL   | INDIRECT_SYMBOL_ABS)) {
            continue;
        }
        uint32_t strtab_offset = symtab[symtab_index].n_un.n_strx;
        char *symbol_name = strtab + strtab_offset;
        if (strnlen(symbol_name, 2) < 2) {
            continue;
        }
        struct rebindings_entry *cur = rebindings;
        while (cur) {
            for (uint j = 0; j < cur->rebindings_nel; j++) {
                if (strcmp(&symbol_name[1], cur->rebindings[j].name) == 0) {
                    if (cur->rebindings[j].replaced != NULL &&
                        indirect_symbol_bindings[i] != cur->rebindings[j].replacement) {
                        *(cur->rebindings[j].replaced) = indirect_symbol_bindings[i];
                    }
                    indirect_symbol_bindings[i] = cur->rebindings[j].replacement;
                    goto symbol_loop;
                }
            }
            cur = cur->next;
        }
    symbol_loop:;
    }
}

static void rebind_symbols_for_image(struct rebindings_entry *rebindings,
                                     const struct mach_header *header,
                                     intptr_t slide) {
    Dl_info info;
    if (dladdr(header, &info) == 0) {
        return;
    }
    
    segment_command_t *cur_seg_cmd;
    segment_command_t *linkedit_segment = NULL;
    struct symtab_command* symtab_cmd = NULL;
    struct dysymtab_command* dysymtab_cmd = NULL;
    
    uintptr_t cur = (uintptr_t)header + sizeof(mach_header_t);
    for (uint i = 0; i < header->ncmds; i++, cur += cur_seg_cmd->cmdsize) {
        cur_seg_cmd = (segment_command_t *)cur;
        if (cur_seg_cmd->cmd == LC_SEGMENT_ARCH_DEPENDENT) {
            if (strcmp(cur_seg_cmd->segname, SEG_LINKEDIT) == 0) {
                linkedit_segment = cur_seg_cmd;
            }
        } else if (cur_seg_cmd->cmd == LC_SYMTAB) {
            symtab_cmd = (struct symtab_command*)cur_seg_cmd;
        } else if (cur_seg_cmd->cmd == LC_DYSYMTAB) {
            dysymtab_cmd = (struct dysymtab_command*)cur_seg_cmd;
        }
    }
    
    if (!symtab_cmd || !dysymtab_cmd || !linkedit_segment ||
        !dysymtab_cmd->nindirectsyms) {
        return;
    }
    
    // Find base symbol/string table addresses
    uintptr_t linkedit_base = (uintptr_t)slide + linkedit_segment->vmaddr - linkedit_segment->fileoff;
    nlist_t *symtab = (nlist_t *)(linkedit_base + symtab_cmd->symoff);
    char *strtab = (char *)(linkedit_base + symtab_cmd->stroff);
    
    // Get indirect symbol table (array of uint32_t indices into symbol table)
    uint32_t *indirect_symtab = (uint32_t *)(linkedit_base + dysymtab_cmd->indirectsymoff);
    
    cur = (uintptr_t)header + sizeof(mach_header_t);
    for (uint i = 0; i < header->ncmds; i++, cur += cur_seg_cmd->cmdsize) {
        cur_seg_cmd = (segment_command_t *)cur;
        if (cur_seg_cmd->cmd == LC_SEGMENT_ARCH_DEPENDENT) {
            if (strcmp(cur_seg_cmd->segname, SEG_DATA) != 0 &&
                strcmp(cur_seg_cmd->segname, SEG_DATA_CONST) != 0) {
                continue;
            }
            for (uint j = 0; j < cur_seg_cmd->nsects; j++) {
                section_t *sect =
                (section_t *)(cur + sizeof(segment_command_t)) + j;
                if ((sect->flags & SECTION_TYPE) == S_LAZY_SYMBOL_POINTERS) {
                    perform_rebinding_with_section(rebindings, sect, slide, symtab, strtab, indirect_symtab);
                }
                if ((sect->flags & SECTION_TYPE) == S_NON_LAZY_SYMBOL_POINTERS) {
                    perform_rebinding_with_section(rebindings, sect, slide, symtab, strtab, indirect_symtab);
                }
            }
        }
    }
}

static void _rebind_symbols_for_image(const struct mach_header *header,
                                      intptr_t slide) {
    rebind_symbols_for_image(_rebindings_head, header, slide);
}

static int fish_rebind_symbols(struct rebinding rebindings[], size_t rebindings_nel) {
    int retval = prepend_rebindings(&_rebindings_head, rebindings, rebindings_nel);
    if (retval < 0) {
        return retval;
    }
    // If this was the first call, register callback for image additions (which is also invoked for
    // existing images, otherwise, just run on existing images
    //首先是遍历 dyld 里的所有的 image，取出 image header 和 slide。注意第一次调用时主要注册 callback
    if (!_rebindings_head->next) {
        _dyld_register_func_for_add_image(_rebind_symbols_for_image);
    } else {
        uint32_t c = _dyld_image_count();
        for (uint32_t i = 0; i < c; i++) {
            _rebind_symbols_for_image(_dyld_get_image_header(i), _dyld_get_image_vmaddr_slide(i));
        }
    }
    return retval;
}


#pragma mark - Record

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <objc/message.h>
#include <objc/runtime.h>
#include <dispatch/dispatch.h>
#include <pthread.h>

static pthread_key_t _thread_key;
__unused static id (*orig_objc_msgSend)(id, SEL, ...);

typedef struct {
    id self; //通过 object_getClass 能够得到 Class 再通过 NSStringFromClass 能够得到类名
    Class cls;
    SEL cmd; //通过 NSStringFromSelector 方法能够得到方法名
    uint64_t time; //us
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
    bool is_main_thread;
} thread_call_stack;

static inline thread_call_stack * get_thread_call_stack() {
    thread_call_stack *cs = (thread_call_stack *)pthread_getspecific(_thread_key);
    if (cs == NULL) {
        cs = (thread_call_stack *)malloc(sizeof(thread_call_stack));
        cs->stack = (thread_call_record *)calloc(128, sizeof(thread_call_record));
        cs->allocated_length = 64;
        cs->index = -1;
        cs->is_main_thread = pthread_main_np();
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
    
    const char *name = sel_getName(_cmd);
    if (strcmp(name, "testParamFunc:") == 0) {
        record->reg_2 = 987654321;
    } else if (strcmp(name, "testAdd:") == 0) {
        record->reg_2 = 888880;
    }
    
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

static inline uintptr_t pop_call_record(uint64_t reg_sp, int *index) {
    thread_call_stack *cs = get_thread_call_stack();
    int nextIndex = cs->index--;
    thread_call_record *pRecord = &cs->stack[nextIndex];
    return pRecord->lr;
}

void before_objc_msgSend(id self, SEL _cmd, uintptr_t lr) {
    uint64_t reg_sp;
    uint64_t reg19;
    __asm volatile("str x19, [%0]\n" :: "r"(&reg19));
    __asm volatile("mov x19, sp");
    __asm volatile("str x19, [%0]\n" :: "r"(&reg_sp));
    __asm volatile("ldr x19, [%0]\n" :: "r"(&reg19));
    
    //sp相差160，除去主动入栈的10个寄存器，剩下的就是函数调用时入栈的
    //PC、LR、SP、FP，加上自身参数3个self、_cmd、lr，两个内部变量reg_sp、reg19，共10个寄存器
    reg_sp = reg_sp + 80;
    
    int index;
    push_call_record(self, object_getClass(self), _cmd, lr, reg_sp, &index);
    reset_params(_cmd, index, reg_sp);
}

uintptr_t after_objc_msgSend() {
    uint64_t reg_sp;
    uint64_t reg19;
    __asm volatile("str x19, [%0]\n" :: "r"(&reg19));
    __asm volatile("mov x19, sp");
    __asm volatile("str x19, [%0]\n" :: "r"(&reg_sp));
    __asm volatile("ldr x19, [%0]\n" :: "r"(&reg19));
    reg_sp = reg_sp + 80;
    
    thread_call_stack *cs = get_thread_call_stack();
    int nextIndex = cs->index--;
    thread_call_record *pRecord = &cs->stack[nextIndex];
    
    uint64_t reg_tmp_19;
    uint64_t reg_tmp_20;
    __asm volatile("str x19, [%0]\n" :: "r"(&reg_tmp_19));
    __asm volatile("str x20, [%0]\n" :: "r"(&reg_tmp_20));
    __asm volatile("ldp x19, x20, [sp, 80]\n");
    __asm volatile("str x19, [%0]\n" :: "r"(&pRecord->reg_0_rt));
    __asm volatile("str x20, [%0]\n" :: "r"(&pRecord->reg_1_rt));
    __asm volatile("ldr x19, [%0]\n" :: "r"(&reg_tmp_19));
    __asm volatile("ldr x20, [%0]\n" :: "r"(&reg_tmp_20));

    const char *name = sel_getName(pRecord->cmd);
    if (strcmp(name, "testAdd:") == 0) {
        pRecord->reg_0_rt = 99999999;
    }

    __asm volatile("str x19, [%0]\n" :: "r"(&reg_tmp_19));
    __asm volatile("str x20, [%0]\n" :: "r"(&reg_tmp_20));
    __asm volatile("ldr x19, [%0]\n" :: "r"(&pRecord->reg_0_rt));
    __asm volatile("ldr x20, [%0]\n" :: "r"(&pRecord->reg_1_rt));
    __asm volatile("stp x19, x20, [sp, 80]\n");
    __asm volatile("ldr x19, [%0]\n" :: "r"(&reg_tmp_19));
    __asm volatile("ldr x20, [%0]\n" :: "r"(&reg_tmp_20));
    
    return pRecord->lr;// 把lr当作返回值，放进x0寄存器
}

//replacement objc_msgSend (arm64)
// https://blog.nelhage.com/2010/10/amd64-and-va_arg/
// http://infocenter.arm.com/help/topic/com.arm.doc.ihi0055b/IHI0055B_aapcs64.pdf
// https://developer.apple.com/library/ios/documentation/Xcode/Conceptual/iPhoneOSABIReference/Articles/ARM64FunctionCallingConventions.html
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
static void hook_Objc_msgSend() {
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

void smCallTraceStart() {
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        pthread_key_create(&_thread_key, &release_thread_call_stack);
        fish_rebind_symbols((struct rebinding[6]){
            {"objc_msgSend", (void *)hook_Objc_msgSend, (void **)&orig_objc_msgSend},
        }, 1);
    });
}

#else

void smCallTraceStart() {}
void smCallConfigMinTime(uint64_t us) {
}
void smCallConfigMaxDepth(int depth) {
}
smCallRecord *smGetCallRecords(int *num) {
    if (num) {
        *num = 0;
    }
    return NULL;
}
void smClearCallRecords() {}

#endif
