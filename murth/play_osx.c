#include <AudioUnit/AudioUnit.h>
#include <CoreMIDI/CoreMIDI.h>
#include <stdio.h>
#include "synth.h"

#define LENGTH_SAMPLES 44100 * 10

#define CHECK_ERR(msg) {if(err!=noErr){printf("Error %d(0x%8x): %s\n", err, err, msg);exit(-1);}}

/*typedef struct
{
  short samples[LENGTH_SAMPLES];
  int writepos;
} ABuffer;
ABuffer audio_buffer;*/

OSStatus audio_cb(void *inRefCon, AudioUnitRenderActionFlags *ioActionFlags,
                  const AudioTimeStamp *inTimeStamp, UInt32 inBusNumber,
                  UInt32 inNumberFrames, AudioBufferList *ioData)
{
  
  AudioBuffer* buf = &ioData->mBuffers[0];
  short* ptr = (short*)buf->mData;
  /*ABuffer* abuf = (ABuffer*)inRefCon;
  while (abuf->writepos < LENGTH_SAMPLES && inNumberFrames > 0)
  {
    *ptr++ = abuf->samples[abuf->writepos++];
    inNumberFrames--;
    if (abuf->writepos == LENGTH_SAMPLES)
      exit(0);
  }*/
  synth(ptr, inNumberFrames);
  return noErr;
}

unsigned current_note = 0;

void midiread_cb(const MIDIPacketList *pktlist,
                 void *readProcRefCon,
                 void *srcConnRefCon)
{
  const MIDIPacket *packet = &pktlist->packet[0];
  for (int i = 0; i < pktlist->numPackets; ++i) {
    
    if (1) {
      printf("%lld: ", packet->timeStamp);
      for (int j = 0; j < packet->length; ++j)
        printf("%02x ", packet->data[j]);
      printf("\n");
    }
    
    
    for (int j = 0; j < packet->length; ++j)
    {
      switch (packet->data[j])
      {
        case 0xb3: // control
          if (packet->data[j+1] == 0x1d && packet->data[j+2] == 0x7f)
            ++current_note;
          else if (packet->data[j+1] == 0x27 && packet->data[j+2] == 0x7f)
            --current_note;
          else
            params[64 + packet->data[j+1]].f = packet->data[j+2] / 127.f;
          current_note &= 63;
          j += 2;
          continue;
        case 0x90: // note down
          params[current_note].f = packet->data[j+1] / 127.f;
          j += 2;
          continue;
        //case 0x80: // note up
        //  continue;
      }
      break; //
    }
    packet = MIDIPacketNext(packet);
  }
}

const unsigned short tick = 10000;

u8 m0[8] = {17, 17, 15, 15, 12, 12, 10, 10};
unsigned short dm0[8] = {0, tick-2, 0, tick-2, 0, tick-2, 0, tick-2};

u8 e0[2] = {127, 0};
unsigned short de0[2] = {100, tick-102};

u8 m1[8] = {22, 22, 17, 17, 15, 15, 12, 12};

u8 m2[18] = {29, 29, 34, 34, 32, 32, 36, 36, 34, 34, 39, 39, 36, 36, 34, 34, 32, 32};
u8 e1[18] = {127, 0, 127, 0, 127, 0, 127, 0, 127, 0, 127, 0, 127, 0, 127, 0, 127, 0};
unsigned short dm2[18] = {
  0, tick*2 - 2,
  0, tick*2 - 2,
  0, tick*2 - 2,
  0, tick*2 - 2,
  0, tick*3 - 2,
  0, tick - 2,
  0, tick - 2,
  0, tick - 2,
  0, tick*2 - 2,
};

paramtbl paramtbls[] = {
  {8, m0, dm0}, {2, e0, de0},
  {8, m1, dm0}, {18, m2, dm2},
  {18, e1, dm2}
};
int nparamtbls = 5;

const unsigned char program[] =
{
  // m0
  0, LDG, 0, LDP, FIU, 36, IADD, NDP, ADD, PHR, 0, STG,      1, LDP, MUL,
  65, LDP, MUL,
  
  // m1
  1, LDG, 2, LDP, FIU, 24, IADD, NDP, ADD, PHR, 1, STG, SIN, 1, LDP, MUL,
  66, LDP, MUL,
  
  // m0 + m1
  ADD,

  // delay (m0 + m1)
  LOAD_SHORT(tick*1.5),
  0, ROF, POP, 0, RRD, 68, LDP, MUL, ADD, 0, RWR,
  
  // m2
  2, LDG, 3, LDP, FIU, NDP, ADD, PHR, 2, STG, SGN, 4, LDP, MUL,
  67, LDP, MUL,
  
  // delay(m2)
  LOAD_SHORT(tick*3),
  1, ROF, POP, 1, RRD, 69, LDP, MUL, ADD, 1, RWR,
  
  // sum all
  ADD,
  
  // postfx
  70, LDP, FIU, 71, LDP, FIU, IMUL,
  2, ROF, POP, 2, RRD, 72, LDP, MUL, ADD, 2, RWR,
  
  //2, LDG, 4, LDP, FIU, /*24, IADD,*/ NDP, ADD, PHR, 2, STG, SGN, /*1, LDP, MUL,*/
  //67, LDP, MUL,
  //ADD,
  
  63, IFU, MUL,
  
  CLAMP, RET
};

int main(int argc, char* argv[])
{
  //synth(audio_buffer.samples, LENGTH_SAMPLES);
  
  AudioComponentDescription ac_desc;
  ac_desc.componentType = kAudioUnitType_Output;
  ac_desc.componentSubType = kAudioUnitSubType_DefaultOutput;
  ac_desc.componentManufacturer = kAudioUnitManufacturer_Apple;
  ac_desc.componentFlags = ac_desc.componentFlagsMask = 0;
  AudioComponent ac = AudioComponentFindNext(0, &ac_desc);
  
  AudioComponentInstance aci;
  OSStatus err = AudioComponentInstanceNew(ac, &aci);
  CHECK_ERR("AudioComponentInstanceNew")
  
  err = AudioUnitInitialize(aci);
  CHECK_ERR("AudioUnitInitialize")
  
  AudioStreamBasicDescription asbd;
  asbd.mSampleRate = 44100;
  asbd.mFormatID = kAudioFormatLinearPCM;
  asbd.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger;
  asbd.mFramesPerPacket = 1;
  asbd.mBitsPerChannel = 16;
  asbd.mChannelsPerFrame = 1;
  asbd.mBytesPerFrame = asbd.mBitsPerChannel * asbd.mChannelsPerFrame / 8;
  asbd.mBytesPerPacket = asbd.mBytesPerFrame * asbd.mFramesPerPacket;
  asbd.mReserved = 0;
  err = AudioUnitSetProperty(aci,
                             kAudioUnitProperty_StreamFormat,
                             kAudioUnitScope_Input,
                             0,
                             &asbd, sizeof(asbd));
  CHECK_ERR("AudioUnitSetProperty")
  
  AURenderCallbackStruct arcs;
  arcs.inputProc = audio_cb;
  //arcs.inputProcRefCon = &audio_buffer;
  
  err = AudioUnitSetProperty(aci,
                             kAudioUnitProperty_SetRenderCallback,
                             kAudioUnitScope_Input,
                             0,
                             &arcs, sizeof(arcs));
  CHECK_ERR("AudioUnitSetProperty")
  
  // midi!
  MIDIClientRef mcli;
  err = MIDIClientCreate(CFSTR("murth"), 0, 0, &mcli);
  CHECK_ERR("MIDIClientCreate");
  MIDIPortRef mport;
  err = MIDIInputPortCreate(mcli, CFSTR("input"), midiread_cb, 0, &mport);
  CHECK_ERR("MIDIInputPortCreate");
  
  unsigned long midi_sources = MIDIGetNumberOfSources();
  for (int i = 0; i < midi_sources; ++i)
    MIDIPortConnectSource(mport, MIDIGetSource(i), 0);
  
  err = AudioOutputUnitStart(aci);
  CHECK_ERR("AudioOutputUnitStart")

  for(;;) sleep(10);
  
#if CLEANDESTROY
  err = AudioComponentInstanceDispose(aci);
  CHECK_ERR("AudioComponentInstanceDispose")
#endif
  
  return 0;
}