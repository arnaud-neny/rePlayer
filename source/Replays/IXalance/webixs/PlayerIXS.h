/*
* Main player API - a facade for the underlying PlayerCore.
*
*   Copyright (C) 2022 Juergen Wothke
*   Copyright (C) original x86 code: Shortcut Software Development BV
*
* LICENSE
*
*   This software is licensed under a CC BY-NC-SA
*   (http://creativecommons.org/licenses/by-nc-sa/4.0/).
*/

#ifndef IXS_PLAYERIXS_H
#define IXS_PLAYERIXS_H

#include "Module.h"

namespace IXS {

  typedef struct PlayerIXS PlayerIXS;

  typedef struct IXS__PlayerIXS__VFTABLE IXS__PlayerIXS__VFTABLE;

  struct IXS__PlayerIXS__VFTABLE {
    char (*loadIxsFileData)(struct PlayerIXS *, byte *, uint, struct FileSFXI *, struct FileSFXI *, float *);

    int (*initAudioOut)(struct PlayerIXS *);

    void (*stopGenAudioThread)(struct PlayerIXS *);

    void (*pauseOutput)(struct PlayerIXS *);

    void (*resumeOutput)(struct PlayerIXS *);

    uint (*getOutputBuffer)(struct PlayerIXS *);

    double (*getWaveProgress)(struct PlayerIXS *);

    char *(*getSongTitle)(struct PlayerIXS *);

    void (*init16kBuf)(struct PlayerIXS *, uint);

    void (*delete0)(struct PlayerIXS *);

    Module *(*createModule)(struct PlayerIXS *);

    void (*setOutputVolume)(struct PlayerIXS *, float);

#if !defined(EMSCRIPTEN) && !defined(LINUX)
    void (*getVolume)(float *, float *);

    void (*setVolume)(float, float);
#else
    uint (*isAudioOutput16Bit)(struct PlayerIXS *);
    uint (*isAudioOutputStereo)(struct PlayerIXS *);
    uint (*getAudioBufferLen)(struct PlayerIXS *);

    byte *(*getAudioBuffer)(struct PlayerIXS *);

    void (*genAudio)(struct PlayerIXS *);

    uint (*getSampleRate)();

    void (*setMixer)(struct PlayerIXS *, bool);

#endif
    byte *(*outputTrackerFile)(struct PlayerIXS *, uint);

    uint32_t (*isSongEnd)(struct PlayerIXS *);  // added feature
  };

  struct PlayerIXS {
    struct IXS__PlayerIXS__VFTABLE *vftable;
    struct PlayerCore *ptrCore_0x4;
  };


  // "public methods"
  PlayerIXS *__cdecl IXS__PlayerIXS__createPlayer_00405d90(uint sampleRate);
}
#endif //IXS_PLAYERIXS_H
