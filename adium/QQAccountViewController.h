//
//  QQAcountViewController.h
//  AdiumQQ
//
//  Created by William Orr on 11/30/11.
//  Copyright (c) 2011 William Orr. All rights reserved.
//

#import <AdiumLibpurple/PurpleAccountViewController.h>

@interface QQAccountViewController : PurpleAccountViewController {
    IBOutlet NSButton* checkBox_tcpConnect;
    IBOutlet NSPopUpButton* popUp_serverList;
}

- (IBAction) changedPreference:(id)sender;


@end
