//
//  MURAppDelegate.h
//  vismurth
//
//  Created by Ivan Avdeev on 09.12.12.
//
//

#import <Cocoa/Cocoa.h>
#import <kapusha/sys/osx/KPView.h>

@interface MURAppDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate>

@property (assign) IBOutlet NSWindow *window;
@property (assign) IBOutlet KPView *view;

@end
