//
//  BAASMHook.h
//  BAASMHook
//
//  Created by BenArvin on 2020/7/23.
//  Copyright © 2020 benarvin. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface BAASMHook : NSObject

+ (void)start;
+ (void)stop;

+ (void)setMonitor:(BOOL)isClassMethod
               cls:(Class)cls
               sel:(SEL)sel
        beforeCall:(void(^)(void))beforeCall
         afterCall:(void(^)(void))afterCall;

+ (void)removeMonitor:(BOOL)isClassMethod
                  cls:(Class)cls
                  sel:(SEL)sel;

@end
