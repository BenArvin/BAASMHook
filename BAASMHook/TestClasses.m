//
//  TestClasses.m
//  BAASMHook
//
//  Created by arvinnie on 2022/10/31.
//  Copyright © 2022 benarvin. All rights reserved.
//

#import "TestClasses.h"

@interface TestDadCalss : NSObject

@end

@implementation TestDadCalss

- (void)testFunc {
    NSLog(@">>>>>>>>> TestDadCalss");
}

- (void)testParamFunc:(NSInteger)intValue {
    NSLog(@">>>>>>>>> TestDadCalss testParamFunc %ld", intValue);
}

@end

@interface TestSonCalss : TestDadCalss

@end

@implementation TestSonCalss

- (void)testFunc {
    NSLog(@">>>>>>>>> TestSonCalss");
    [super testFunc];
}

- (void)testParamFunc:(NSInteger)intValue {
    NSLog(@">>>>>>>>> TestSonCalss testParamFunc %ld", intValue);
    [super testParamFunc:intValue];
}

@end

@interface TestSonSonCalss : TestSonCalss

@end

@implementation TestSonSonCalss

- (void)testFunc {
    NSLog(@">>>>>>>>> TestSonSonCalss");
    [super testFunc];
}

- (void)testParamFunc:(NSInteger)intValue {
    NSLog(@">>>>>>>>> TestSonSonCalss testParamFunc %ld", intValue);
    [super testParamFunc:intValue];
}

- (void)testParamFunc:(NSInteger)intValue1 intValue2:(NSInteger)intValue2 intValue3:(NSInteger)intValue3 intValue4:(NSInteger)intValue4 intValue5:(NSInteger)intValue5 intValue6:(NSInteger)intValue6 intValue7:(NSInteger)intValue7 intValue8:(NSInteger)intValue8 intValue9:(NSInteger)intValue9 intValue10:(NSInteger)intValue10 {
}

- (int)add:(int)num1 num2:(int)num2 {
    return num1 + num2;
}

@end

@implementation TestClasses

+ (void)startTest {
    TestSonSonCalss *sonSon = [[TestSonSonCalss alloc] init];
    [sonSon testFunc];
//    [sonSon testParamFunc:123 intValue2:234 intValue3:345 intValue4:456 intValue5:567 intValue6:678 intValue7:789 intValue8:1234 intValue9:2345 intValue10:3456];
    [sonSon testParamFunc:123456789];
    
    int result = [sonSon add:1 num2:2];
    NSLog(@"1+2 result=%d", result);
}

@end
