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
                                                   encoding:NSUTF8StringEncoding error:nil]).UTF8String, samplerate, 120);
  murth_set_midi_note_handler("midinote", -1, -1);
  murth_set_midi_control_handler("midi_trivol", -1, 1, -1);
  murth_set_midi_control_handler("midi_sinevol", -1, 2, -1);
  murth_set_midi_control_handler("midi_sqvol", -1, 3, -1);
  murth_set_midi_control_handler("midi_delay0wet", -1, 4, -1);
  murth_set_midi_control_handler("midi_delay1wet", -1, 5, -1);
  murth_set_midi_control_handler("midi_delayMwet", -1, 6, -1);
  murth_set_midi_control_handler("midi_delayM", -1, 7, -1);
  osx_audio_start();
}
- (BOOL)windowShouldClose:(id)sender {
  osx_audio_close();
  [[NSApplication sharedApplication] terminate:self];
  return YES;
}


@end
