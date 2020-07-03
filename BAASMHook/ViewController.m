//
//  ViewController.m
//  BAASMHook
//
//  Created by arvinnie on 2020/7/3.
//  Copyright © 2020 benarvin. All rights reserved.
//

#import "ViewController.h"

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

@end

@interface ViewController ()

@end

@implementation ViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    
    TestSonSonCalss *sonSon = [[TestSonSonCalss alloc] init];
    [sonSon testFunc];
    [sonSon testParamFunc:789];
}


@end
