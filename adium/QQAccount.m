//
//  QQAccount.m
//  libqq-adium
//
//  Created by William Orr on 11/19/11.
//  Copyright (c) William Orr. All rights reserved.
//

#import "QQAccount.h"
#import "QQService.h"
#include <stdlib.h>

@implementation WebQQAccount

- (void) initAccount {
    [super initAccount];
    [self setPreference:@"web2.qq.com" forKey:KEY_CONNECT_HOST group:GROUP_ACCOUNT_STATUS]  ;
}

- (const char*) protocolPlugin {
    return "prpl-webqq";
}

@end
