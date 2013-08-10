//
//  QQService.m
//  libqq-adium
//
//  Created by William Orr on 11/19/11.
//  Copyright (c) 2011 William Orr. All rights reserved.
//

#import "QQService.h"

#import "QQAccount.h"
#import "QQAccountViewController.h"

#import <Adium/AIStatusControllerProtocol.h>
#import <Adium/AISharedAdium.h> 
#import <AIUtilities/AIImageAdditions.h>

@implementation WebQQService

- (void) registerStatuses {
    [[adium statusController] registerStatus:STATUS_NAME_AVAILABLE 
                             withDescription:[[adium statusController] localizedDescriptionForCoreStatusName:STATUS_NAME_AVAILABLE] 
                             ofType:AIAvailableStatusType
                             forService:self];
    [[adium statusController] registerStatus:STATUS_NAME_AWAY
                             withDescription:[[adium statusController]
                                localizedDescriptionForCoreStatusName:STATUS_NAME_AWAY]
                             ofType:AIAwayStatusType 
                             forService:self];
    [[adium statusController] registerStatus:STATUS_NAME_INVISIBLE
                             withDescription:[[adium statusController]
                                localizedDescriptionForCoreStatusName:STATUS_NAME_INVISIBLE]
                             ofType:AIInvisibleStatusType 
                             forService:self];
    [[adium statusController] registerStatus:STATUS_NAME_BUSY
							 withDescription:[[adium statusController] localizedDescriptionForCoreStatusName:STATUS_NAME_BUSY]
                             ofType:AIAwayStatusType
                             forService:self];
}

- (Class) accountClass {
    return [WebQQAccount class];
}
/*
- (AIAccountViewController*) accountViewController {
    return [QQAccountViewController accountViewController];
}
*/
- (BOOL)isSocialNetworkingService {
    return TRUE;
}
//isSocialNetworkingService
- (NSString*) serviceCodeUniqueID {
    return @"libpurple-webqq";
}

- (NSString*) serviceID {
    return @"WebQQ";
}

- (NSString*) serviceClass {
    return @"WebQQ";
}

- (NSString*) shortDescription {
    return @"WebQQ";
}

- (NSString*) longDescription {
    return @"WebQQ";
}

- (NSString*) userNameLabel {
    return @"QQ";
}

- (NSString*) contactUserNameLabel {
    return @"Contact UID";
}

- (NSCharacterSet*) allowedCharactersForAccountName {
    return [NSCharacterSet alphanumericCharacterSet];
}

- (NSCharacterSet*) allowedCharactersForUIDs {
    return [NSCharacterSet decimalDigitCharacterSet];
}

- (BOOL) caseSensitive {
    return YES;
}

- (BOOL) requiresPassword {
    return YES;
}

- (NSUInteger) allowedLengthForUIDs {
    return 12;
}

- (BOOL) canCreateGroupChats {
    return YES;
}

- (AIServiceImportance) serviceImportance {
    return AIServiceSecondary;
}

@end
