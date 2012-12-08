#include <AudioUnit/AudioUnit.h>
#include <stdio.h>
#include "synth.h"

#define LENGTH_SAMPLES 44100 * 10

#define CHECK_ERR(msg) {if(err!=noErr){printf("Error %d(0x%8x): %s\n", err, err, msg);exit(-1);}}

typedef struct
{
  short samples[LENGTH_SAMPLES];
  int writepos;
} ABuffer;
ABuffer audio_buffer;

OSStatus audio_cb(void *inRefCon, AudioUnitRenderActionFlags *ioActionFlags,
                  const AudioTimeStamp *inTimeStamp, UInt32 inBusNumber,
                  UInt32 inNumberFrames, AudioBufferList *ioData)
{
  ABuffer* abuf = (ABuffer*)inRefCon;
  AudioBuffer* buf = &ioData->mBuffers[0];
  short* ptr = (short*)buf->mData;
  while (abuf->writepos < LENGTH_SAMPLES && inNumberFrames > 0)
  {
    *ptr++ = abuf->samples[abuf->writepos++];
    inNumberFrames--;
    if (abuf->writepos == LENGTH_SAMPLES)
      exit(0);
  }
  return noErr;
}

const unsigned char program[] =
{
  0, LDG, 0, LDP, FIU, 24, IADD, NDP, ADD, PHR, 0, STG, SIN,
  1, LDG, 1, LDP, FIU, 24, IADD, NDP, ADD, PHR, 1, STG, SIN,
  2, LDG, 2, LDP, FIU, 24, IADD, NDP, ADD, PHR, 2, STG, SIN,
  ADD, 42, IFU, MUL,
  RET
};

int main(int argc, char* argv[])
{
  synth(audio_buffer.samples, LENGTH_SAMPLES);
  
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
  arcs.inputProcRefCon = &audio_buffer;
  
  err = AudioUnitSetProperty(aci,
                             kAudioUnitProperty_SetRenderCallback,
                             kAudioUnitScope_Input,
                             0,
                             &arcs, sizeof(arcs));
  CHECK_ERR("AudioUnitSetProperty")
  
  err = AudioOutputUnitStart(aci);
  CHECK_ERR("AudioOutputUnitStart")

  sleep(10);
  
#if CLEANDESTROY
  err = AudioComponentInstanceDispose(aci);
  CHECK_ERR("AudioComponentInstanceDispose")
#endif
  
  return 0;
}