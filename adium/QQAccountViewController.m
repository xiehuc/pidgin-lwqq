//
//  QQAccountViewController.m
//  AdiumQQ
//
//  Created by William Orr on 11/30/11.
//  Copyright (c) 2011 William Orr. All rights reserved.
//

#import "QQAccountViewController.h"

#import "QQAccount.h"
#import "QQService.h"

#include <libpurple/account.h>

@interface QQAccountViewController ()
- (NSMenu*) serverMenu;
@end

@implementation QQAccountViewController

- (NSString *)nibName {
    return @"QQAccountView";
}

- (void) configureForAccount:(AIAccount *)inAccount {
    [super configureForAccount:inAccount];
    
   // [checkBox_tcpConnect setState:[[account preferenceForKey:KEY_QQ_TCP_CONNECT group:GROUP_ACCOUNT_STATUS] boolValue]];

    [popUp_serverList setMenu:[self serverMenu]];
    [popUp_serverList selectItemWithTitle:[account preferenceForKey:KEY_CONNECT_HOST group:GROUP_ACCOUNT_STATUS]];
    [textField_connectPort setIntegerValue:[[account preferenceForKey:KEY_CONNECT_PORT group:GROUP_ACCOUNT_STATUS] intValue]];
}

- (void) saveConfiguration {
    [super saveConfiguration];
    PurpleAccount* purpAcc;
    
    /*if ([account respondsToSelector:@selector(purpleAccount)]) {
        purpAcc = [account purpleAccount];
    }

    BOOL tcp = [checkBox_tcpConnect state];
    [account setPreference:[NSNumber numberWithInt:tcp] forKey:KEY_QQ_TCP_CONNECT group:GROUP_ACCOUNT_STATUS];
    
    purple_account_set_bool(purpAcc,
                            [KEY_QQ_TCP_CONNECT UTF8String],
                            tcp);*/

    NSString* server = [popUp_serverList titleOfSelectedItem];
    [account setPreference:server forKey:KEY_CONNECT_HOST group:GROUP_ACCOUNT_STATUS];
    
    NSString* port = [textField_connectPort stringValue];
    [account setPreference:[NSNumber numberWithInt:[port intValue]] forKey:KEY_CONNECT_PORT group:GROUP_ACCOUNT_STATUS];
    
    // Let's send over the preference information to libqq-pidgin
    server = [[server stringByAppendingString:@":"] stringByAppendingString:port];
    /*purple_account_set_string(purpAcc, 
                               [KEY_QQ_CONNECT_HOST UTF8String], 
                               [server UTF8String]);*/
}

- (void) changedPreference:(id)sender {
    if ([checkBox_tcpConnect isEqual:sender])
        [popUp_serverList setMenu:[self serverMenu]];
}

- (void) awakeFromNib {
    [super awakeFromNib];
    [popUp_serverList setMenu:[self serverMenu]];
}

- (NSMenu*) serverMenu {
    /*NSMenu* serverMenu = [[NSMenu allocWithZone:[NSMenu zone]] init];
    NSArray* serverArray;
    NSMenuItem* serverMenuItem;
    NSEnumerator* enumer;
    NSString* item;
    
    serverArray = [QQService getServerList:[checkBox_tcpConnect state]];
    
    enumer = [serverArray objectEnumerator];
    while (item = [enumer nextObject]) {
        serverMenuItem = [[[NSMenuItem allocWithZone:[NSMenu zone]] initWithTitle:item action:nil keyEquivalent:@""] autorelease];
        [serverMenu addItem:serverMenuItem];
    }
    
    return [serverMenu autorelease];*/
    return NULL;
}



@end
