//
//  MURAppDelegate.m
//  vismurth
//
//  Created by Ivan Avdeev on 09.12.12.
//
//

#import "MURAppDelegate.h"
#import "kapusha/core/Log.h"

extern kapusha::IViewport *makeViewport();

class CocoaLog : public kapusha::Log::ISystemLog
{
public:
  virtual void write(const char* msg)
  {
    NSLog(@"%s", msg);
  }
};

@implementation MURAppDelegate

- (void)dealloc
{
    [super dealloc];
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
  //! \hack force likning KPView class from libkapusha
  [KPView class];
  
  KP_LOG_OPEN("Kapusha.log", new CocoaLog);

  [self.view setViewport:makeViewport()];
}

- (BOOL)windowShouldClose:(id)sender
{
  [[NSApplication sharedApplication] terminate:self];
  return YES;
}


@end
