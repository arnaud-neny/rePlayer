//============================================================================
//
//   SSSS    tt          lll  lll       
//  SS  SS   tt           ll   ll        
//  SS     tttttt  eeee   ll   ll   aaaa 
//   SSSS    tt   ee  ee  ll   ll      aa
//      SS   tt   eeeeee  ll   ll   aaaaa  --  "An Atari 2600 VCS Emulator"
//  SS  SS   tt   ee      ll   ll  aa  aa
//   SSSS     ttt  eeeee llll llll  aaaaa
//
// Copyright (c) 1995-2015 by Bradford W. Mott, Stephen Anthony
// and the Stella Team
//
// See the file "License.txt" for information on usage and redistribution of
// this file, and for a DISCLAIMER OF ALL WARRANTIES.
//
// $Id: SoundSDL2.cxx 3205 2015-09-14 21:33:50Z stephena $
//============================================================================

#include <sstream>
#include <cassert>
#include <cmath>

#include "TIASnd.h"
#include "SoundSDL2.h"

namespace Emulation {

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
SoundSDL2::SoundSDL2(TIASound *tiasound)
  : myTIASound(tiasound),
    myIsEnabled(false),
    myIsInitializedFlag(false),
    myLastRegisterSetCycle(0),
    myNumChannels(2),
    myFragmentSizeLogBase2(0),
    myIsMuted(true),
    myVolume(100)
{
  // The sound system is opened only once per program run, to eliminate
  // issues with opening and closing it multiple times
  // This fixes a bug most prevalent with ATI video cards in Windows,
  // whereby sound stopped working after the first video change
  SDL_AudioSpec desired;
  desired.freq   = 44100;
  desired.format = 0;// AUDIO_S16SYS;
  desired.channels = 2;
  desired.samples  = 1024;
  desired.callback = callback;
  desired.userdata = static_cast<void*>(this);

  myHardwareSpec = desired;
/*
  if(SDL_OpenAudio(&desired, &myHardwareSpec) < 0)
  {
    std::cerr << "WARNING: Couldn't open SDL audio system!\n"
        << "         " << SDL_GetError() << "\n";
    return;
  }
*/

  // Make sure the sample buffer isn't to big (if it is the sound code
  // will not work so we'll need to disable the audio support)
/*
  if((float(myHardwareSpec.samples) / float(myHardwareSpec.freq)) >= 0.25)
  {
    std::cerr << "WARNING: Sound device doesn't support realtime audio! Make "
        << "sure a sound\n"
        << "         server isn't running.  Audio is disabled.\n";
    SDL_CloseAudio();
    return;
  }
*/

  // Pre-compute fragment-related variables as much as possible
  myFragmentSizeLogBase2 = log(myHardwareSpec.samples) / log(2.0);
  myFragmentSizeLogDiv1 = myFragmentSizeLogBase2 / 60.0;
  myFragmentSizeLogDiv2 = (myFragmentSizeLogBase2 - 1) / 60.0;

  myIsInitializedFlag = true;
//   SDL_PauseAudio(1);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
SoundSDL2::~SoundSDL2()
{
  // Close the SDL audio system if it's initialized
  if(myIsInitializedFlag)
  {
//     SDL_CloseAudio();
    myIsEnabled = myIsInitializedFlag = false;
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void SoundSDL2::setEnabled(bool)
{
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void SoundSDL2::open()
{
  myIsEnabled = false;
  mute(true);
  if(!myIsInitializedFlag)
  {
    return;
  }

  // Now initialize the TIASound object which will actually generate sound
  myTIASound->outputFrequency(myHardwareSpec.freq);
//   const string& chanResult =
      myTIASound->channels(myHardwareSpec.channels, myNumChannels == 2);

  // Adjust volume to that defined in settings
  myVolume = 100;
  setVolume(myVolume);

  // And start the SDL sound subsystem ...
  myIsEnabled = true;
  mute(false);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void SoundSDL2::close()
{
  if(myIsInitializedFlag)
  {
    myIsEnabled = false;
//     SDL_PauseAudio(1);
    myLastRegisterSetCycle = 0;
    myTIASound->reset();
    myRegWriteQueue.clear();
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void SoundSDL2::mute(bool state)
{
  if(myIsInitializedFlag)
  {
    myIsMuted = state;
//     SDL_PauseAudio(myIsMuted ? 1 : 0);
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void SoundSDL2::reset()
{
  if(myIsInitializedFlag)
  {
//     SDL_PauseAudio(1);
    myLastRegisterSetCycle = 0;
    myTIASound->reset();
    myRegWriteQueue.clear();
    mute(myIsMuted);
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void SoundSDL2::setVolume(Int32 percent)
{
  if(myIsInitializedFlag && (percent >= 0) && (percent <= 100))
  {
//     SDL_LockAudio();
    myVolume = percent;
    myTIASound->volume(percent);
//     SDL_UnlockAudio();
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void SoundSDL2::adjustVolume(Int8 direction)
{
  string message;

  Int32 percent = myVolume;

  if(direction == -1)
    percent -= 2;
  else if(direction == 1)
    percent += 2;

  if((percent < 0) || (percent > 100))
    return;

  setVolume(percent);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void SoundSDL2::adjustCycleCounter(Int32 amount)
{
  myLastRegisterSetCycle += amount;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void SoundSDL2::setChannels(uInt32 channels)
{
  if(channels == 1 || channels == 2)
    myNumChannels = channels;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void SoundSDL2::setFrameRate(float framerate)
{
  // Recalculate since frame rate has changed
  // FIXME - should we clear out the queue or adjust the values in it?
  myFragmentSizeLogDiv1 = myFragmentSizeLogBase2 / framerate;
  myFragmentSizeLogDiv2 = (myFragmentSizeLogBase2 - 1) / framerate;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void SoundSDL2::set(uInt16 addr, uInt8 value, Int32 cycle)
{
//   SDL_LockAudio();

  // First, calculate how many seconds would have past since the last
  // register write on a real 2600
  double delta = double(cycle - myLastRegisterSetCycle) / 1193191.66666667;

  // Now, adjust the time based on the frame rate the user has selected. For
  // the sound to "scale" correctly, we have to know the games real frame 
  // rate (e.g., 50 or 60) and the currently emulated frame rate. We use these
  // values to "scale" the time before the register change occurs.
  RegWrite info;
  info.addr = addr;
  info.value = value;
  info.delta = delta;
  myRegWriteQueue.enqueue(info);

  // Update last cycle counter to the current cycle
  myLastRegisterSetCycle = cycle;

//   SDL_UnlockAudio();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void SoundSDL2::processFragment(Int16* stream, uInt32 length)
{
  uInt32 channels = myHardwareSpec.channels;
//   length = length / channels;

  // If there are excessive items on the queue then we'll remove some
/*
  if(myRegWriteQueue.duration() > myFragmentSizeLogDiv1)
  {
    double removed = 0.0;
    while(removed < myFragmentSizeLogDiv2)
    {
      RegWrite& info = myRegWriteQueue.front();
      removed += info.delta;
      myTIASound->set(info.addr, info.value);
      myRegWriteQueue.dequeue();
    }
  }
*/

  double position = 0.0;
  double remaining = length;

  while(remaining > 0.0)
  {
    if(myRegWriteQueue.size() == 0)
    {
      // There are no more pending TIA sound register updates so we'll
      // use the current settings to finish filling the sound fragment
      myTIASound->process(stream + (uInt32(position) * channels),
          length - uInt32(position));

      // Since we had to fill the fragment we'll reset the cycle counter
      // to zero.  NOTE: This isn't 100% correct, however, it'll do for
      // now.  We should really remember the overrun and remove it from
      // the delta of the next write.
      myLastRegisterSetCycle = 0;
      break;
    }
    else
    {
      // There are pending TIA sound register updates so we need to
      // update the sound buffer to the point of the next register update
      RegWrite& info = myRegWriteQueue.front();

      // How long will the remaining samples in the fragment take to play
      double duration = remaining / myHardwareSpec.freq;

      // Does the register update occur before the end of the fragment?
      if(info.delta <= duration)
      {
        // If the register update time hasn't already passed then
        // process samples upto the point where it should occur
        if(info.delta > 0.0)
        {
          // Process the fragment upto the next TIA register write.  We
          // round the count passed to process up if needed.
          double samples = (myHardwareSpec.freq * info.delta);
          myTIASound->process(stream + (uInt32(position) * channels),
              uInt32(samples) + uInt32(position + samples) - 
              (uInt32(position) + uInt32(samples)));

          position += samples;
          remaining -= samples;
        }
        myTIASound->set(info.addr, info.value);
        myRegWriteQueue.dequeue();
      }
      else
      {
        // The next register update occurs in the next fragment so finish
        // this fragment with the current TIA settings and reduce the register
        // update delay by the corresponding amount of time
        myTIASound->process(stream + (uInt32(position) * channels),
            length - uInt32(position));
        info.delta -= duration;
        break;
      }
    }
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void SoundSDL2::callback(void* udata, uInt8* stream, int len)
{
  SoundSDL2* sound = static_cast<SoundSDL2*>(udata);
  if(sound->myIsEnabled)
  {
    // The callback is requesting 8-bit (unsigned) data, but the TIA sound
    // emulator deals in 16-bit (signed) data
    // So, we need to convert the pointer and half the length
    sound->processFragment(reinterpret_cast<Int16*>(stream), uInt32(len) >> 1);
  }
  else
    memset(stream, 0, len);  // Write 'silence'
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
SoundSDL2::RegWriteQueue::RegWriteQueue(uInt32 capacity)
  : myCapacity(capacity),
    myBuffer(0),
    mySize(0),
    myHead(0),
    myTail(0)
{
    myBuffer = new RegWrite[myCapacity];
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
SoundSDL2::RegWriteQueue::~RegWriteQueue()
{
  delete[] myBuffer;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void SoundSDL2::RegWriteQueue::clear()
{
  myHead = myTail = mySize = 0;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void SoundSDL2::RegWriteQueue::dequeue()
{
  if(mySize > 0)
  {
    myHead = (myHead + 1) % myCapacity;
    --mySize;
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
double SoundSDL2::RegWriteQueue::duration() const
{
  double duration = 0.0;
  for(uInt32 i = 0; i < mySize; ++i)
  {
    duration += myBuffer[(myHead + i) % myCapacity].delta;
  }
  return duration;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void SoundSDL2::RegWriteQueue::enqueue(const RegWrite& info)
{
  // If an attempt is made to enqueue more than the queue can hold then
  // we'll enlarge the queue's capacity.
  if(mySize == myCapacity)
    grow();

  myBuffer[myTail] = info;
  myTail = (myTail + 1) % myCapacity;
  ++mySize;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
SoundSDL2::RegWrite& SoundSDL2::RegWriteQueue::front() const
{
  assert(mySize != 0);
  return myBuffer[myHead];
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
uInt32 SoundSDL2::RegWriteQueue::size() const
{
  return mySize;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void SoundSDL2::RegWriteQueue::grow()
{
  RegWrite *buffer = new RegWrite[myCapacity*2];
  for(uInt32 i = 0; i < mySize; ++i) {
      buffer[i] = myBuffer[(myHead + i) % myCapacity];
  }

  myHead = 0;
  myTail = mySize;
  myCapacity *= 2;
  delete[] myBuffer;
  myBuffer = buffer;
}

}
