#include "../murth/osx.h"
#include "../murth/murth.h"
#import "MURAppDelegate.h"
#import "kapusha/core/Log.h"

extern kapusha::IViewport *makeViewport();

class CocoaLog : public kapusha::Log::ISystemLog {
public:
  virtual void write(const char* msg) {
    NSLog(@"%s", msg);
  }
};

@interface MURAppDelegate ()
@property (strong) NSString *playFile;
@end

@implementation MURAppDelegate
- (void)dealloc {
    [super dealloc];
}
- (BOOL)application:(NSApplication *)theApplication
           openFile:(NSString *)filename
{
  self.playFile = filename;
  return YES;
}
- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
  //! \hack force likning KPView class from libkapusha
  [KPView class];
  
  KP_LOG_OPEN("Kapusha.log", new CocoaLog);

  [self.view setViewport:makeViewport()];
  int samplerate;
  osx_audio_init(&samplerate);
  murth_init(((NSString*)[NSString stringWithContentsOfFile:self.playFile
                                                   encoding:NSUTF8StringEncoding error:nil]).UTF8String, samplerate, 90);
  murth_set_midi_note_handler("midinote", -1, -1);
  murth_set_midi_control_handler("ctrl_notelength", -1, -1, -1);
  osx_audio_start();
}
- (BOOL)windowShouldClose:(id)sender {
  osx_audio_close();
  [[NSApplication sharedApplication] terminate:self];
  return YES;
}


@end