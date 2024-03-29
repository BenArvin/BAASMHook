//
//  BAASMHook.m
//  BAASMHook
//
//  Created by BenArvin on 2020/7/23.
//  Copyright © 2020 benarvin. All rights reserved.
//

#import "BAASMHook.h"
#import "BAASMHookCore.h"

static Class kBAASMHookExceptionClassOSObject;

@interface BAASMHook() {
}

+ (BAASMHook *)shared;
- (void)onBeforeCall:(Class)cls sel:(SEL)sel;
- (void)onAfterCall:(Class)cls sel:(SEL)sel;
- (void)testFunc;

@end

void _onBeforeCall(Class cls, SEL sel) {
    if ([cls isSubclassOfClass:kBAASMHookExceptionClassOSObject]) {
        return;
    }
    printf(">>>>>>>>>>>>>>>>>>>  _onBeforeCall\n");
    printf(class_getName(cls));
    printf("\n");
    printf(sel_getName(sel));
    printf("\n");
    [[BAASMHook shared] onBeforeCall:cls sel:sel];
}

void _onAfterCall(Class cls, SEL sel) {
//    if ([cls isSubclassOfClass:kBAASMHookExceptionClassOSObject]) {
//        return;
//    }
//    const char *clsName = class_getName(cls);
//    const char *selName = sel_getName(sel);
//    printf(">>>>>>>>>>>>>>>>>>>  _onAfterCall\n");
//    printf(clsName);
//    printf("\n");
//    printf(selName);
//    printf("\n");
//    [BAASMHook shared];
//    [[BAASMHook shared] onAfterCall:cls sel:sel];
}

@implementation BAASMHook

+ (BAASMHook *)shared {
    static dispatch_once_t onceToken;
    static BAASMHook *_shared;
    dispatch_once(&onceToken, ^{
        kBAASMHookExceptionClassOSObject = NSClassFromString(@"OS_object");
        kBAASMHookExceptionClassOSObject = [NSObject class];
        
        _shared = [[BAASMHook alloc] init];
    });
    return _shared;
}

- (instancetype)init {
    self = [super init];
    if (self) {
        setCallback(_onBeforeCall, _onAfterCall);
    }
    return self;
}

#pragma mark - public methods
+ (void)start {
    [[self shared] startAction];
}

+ (void)stop {
    [[self shared] stopAction];
}

+ (void)setMonitor:(BOOL)isClassMethod cls:(Class)cls sel:(SEL)sel beforeCall:(void(^)(void))beforeCall afterCall:(void(^)(void))afterCall {
    
}

+ (void)removeMonitor:(BOOL)isClassMethod cls:(Class)cls sel:(SEL)sel {
    
}

+ (void)afterCall:(Class)cls sel:(SEL)sel {
    NSLog(@"");
}

#pragma mark - private methods
- (void)startAction {
    startAsmHook();
}

- (void)stopAction {
    stopAsmHook();
}

- (void)onBeforeCall:(Class)cls sel:(SEL)sel {
//    NSLog(@"-------  onBeforeCall");
}

- (void)onAfterCall:(Class)cls sel:(SEL)sel {
    NSLog(@"-------  onAfterCall");
}

- (void)testFunc {
    
}

@end
