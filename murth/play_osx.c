#include <AudioUnit/AudioUnit.h>
#include <CoreMIDI/CoreMIDI.h>
#include <stdio.h>
#include "synth.h"

#define CHECK_ERR(msg) {if(err!=noErr){printf("Error %d(0x%8x): %s\n", err, err, msg);exit(-1);}}

OSStatus audio_cb(void *inRefCon, AudioUnitRenderActionFlags *ioActionFlags,
                  const AudioTimeStamp *inTimeStamp, UInt32 inBusNumber,
                  UInt32 inNumberFrames, AudioBufferList *ioData) {
  AudioBuffer* buf = &ioData->mBuffers[0];
  short* ptr = (short*)buf->mData;
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

void audio_play()
{
  for (int i = 0; i < MAX_PARAMS; ++i) params[i].f = .5f;
  
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
  CHECK_ERR("AudioOutputUnitStart");
}

void audio_stop()
{
#if CLEANDESTROY
  err = AudioComponentInstanceDispose(aci);
  CHECK_ERR("AudioComponentInstanceDispose")
#endif
}
