//
//  QQPlugin.m
//  libqq-adium
//
//  Created by William Orr on 11/19/11.
//  Copyright (c) 2011 William Orr. All rights reserved.
//

#import "QQPlugin.h"
#import "QQService.h" 
#import <glib.h>
#import <stdio.h>
#import "lwqq.h"

extern gboolean purple_init_webqq_plugin(void);
@implementation WebQQPlugin

- (void) installPlugin {
    [WebQQService registerService];
    purple_init_webqq_plugin();
}

- (void) uninstallPlugin {
}

- (void) installLibpurplePlugin 
{
    
}

- (void) loadLibpurplePlugin {
}

- (NSString*) pluginAuthor {
    return @"xiehuc";
}

- (NSString*) pluginVersion {
    return @"0.2b";
}

- (NSString*) pluginDescription {
    return @"the port for pidgin-lwqq to adium";
}

- (NSString*) pluginWebsite {
    return @"www.github.com/xiehuc/pidgin-lwqq";
}

@end
