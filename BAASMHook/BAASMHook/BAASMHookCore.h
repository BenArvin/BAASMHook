//
//  BAASMHookCore.h
//  BAASMHook
//
//  Created by BenArvin on 2017/7/16.
//  Copyright © 2020年 BenArvin. All rights reserved.
//

#include <objc/runtime.h>

extern void startAsmHook(void);
extern void stopAsmHook(void);
void setCallback(void(*onBeforeCall)(Class, SEL), void(*onAfterCall)(Class, SEL));
