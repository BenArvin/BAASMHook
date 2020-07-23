//
//  BAASMCallbackManager.h
//  BAASMHook
//
//  Created by arvinnie on 2020/7/23.
//  Copyright © 2020 benarvin. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface BAASMCallbackManager : NSObject

+ (void)onASMBeforeCall:(Class)cls sel:(SEL)sel;

@end
