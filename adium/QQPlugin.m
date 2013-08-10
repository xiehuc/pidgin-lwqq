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
    return @"William Orr";
}

- (NSString*) pluginVersion {
    return @"0.7";
}

- (NSString*) pluginDescription {
    return @"Allows Adium to connect to QQ accounts";
}

- (NSString*) pluginWebsite {
    return @"www.worrbase.com";
}

@end
