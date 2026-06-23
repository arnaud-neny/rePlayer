// TFMX audio decoder library by Michael Schwendt

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, see <https://www.gnu.org/licenses/>.

#include "TFMXDecoder.h"

#include <cstring>

namespace tfmxaudiodecoder {

class PaulaVoice;

const std::string TFMXDecoder::FORMAT_NAME = "TFMX/Huelsbeck (AMIGA)";
const std::string TFMXDecoder::FORMAT_NAME_PRO = "TFMX Pro/Huelsbeck (AMIGA)";
const std::string TFMXDecoder::FORMAT_NAME_7V = "TFMX 7V/Huelsbeck (AMIGA)";
const std::string TFMXDecoder::TAG = "TFMX";
const std::string TFMXDecoder::TAG_TFMXSONG = "TFMX-SONG ";
const std::string TFMXDecoder::TAG_TFMXSONG_LC = "tfmxsong";

TFMXDecoder::TFMXDecoder() {
    input.buf = 0;
    input.bufLen = input.len = 0;
    input.smplLoaded = false;

    admin.initialized = false;

    // Set up some dummy voices to decouple the decoder from the mixer.
    for (ubyte v=0; v<VOICES_MAX; v++) {
        voiceVars[v].ch = &dummyVoices[v];
    }

    loopMode = true;// rePlayer
    srand( reinterpret_cast<uintptr_t>(this)&0xffff );
}

TFMXDecoder::~TFMXDecoder() {
    delete[] input.buf;
}

// ----------------------------------------------------------------------

// Assign default values to essential runtime variables.
void TFMXDecoder::reset() {
    cmd.aa = cmd.bb = cmd.cd = cmd.ee = 0;

    for (ubyte t=0; t<TRACKS_MAX; t++) {
        Track& tr = track[t];
        tr.on = false;
        tr.PT = 0xff; tr.TR = 0;
        tr.pattern.offset = tr.pattern.step = 0;
        tr.pattern.wait = 0;
        tr.pattern.loops = -1;
    }
    for (ubyte v=0; v<VOICES_MAX; v++) {
        VoiceVars& voice = voiceVars[v];
        
        voice.voiceNum = v;
        voice.envelope.flag = 0;
        voice.portamento.speed = 0;
        voice.keyUp = true;
        voice.vibrato.time = voice.vibrato.delta = 0;
        voice.waitOnDMACount = -1;
        voice.waitOnDMAPrevLoops = 0;

        voice.addBeginCount = voice.addBeginArg = 0;
        voice.addBeginOffset = 0;

        voice.period = voice.outputPeriod = 0;
        voice.detune = 0;
        voice.volume = voice.outputVolume = 0;

        voice.note = voice.notePrevious = 0;
        voice.noteVolume = 0;

        voice.macro.wait = 1;
        voice.macro.step = 0;
        voice.macro.state = 0;
        voice.macro.loop = 0xff;
        voice.macro.extraWait = true;
        voice.macro.delayedOff = voice.macro.delayedOn = false;
        voice.macro.offset = 0;
        voice.macro.branchIfSame = false;
        
        voice.sid.targetOffset = 0x100*v + 4;
        voice.sid.targetLength = 0;
        voice.sid.lastSample = 0;
        voice.sid.op1.interDelta = 0;
        
        voice.rnd.flag = 0;

        voice.ch->off();
        toPaulaLength(voice,1);
        toPaulaStart(voice,offsets.silence);
        voice.ch->paula.volume = 0;
        voice.ch->paula.period = 0;
    }

    for (int m=0; m<=TRACK_CMD_MAX; m++) {
        trackCmdUsed[m] = false;
    }
    for (int m=0; m<16; m++) {
        patternCmdUsed[m] = false;
    }
    for (int m=0; m<0x40; m++) {
        macroCmdUsed[m] = false;
    }
}

ubyte TFMXDecoder::getVoices() {
    return voices;
}
    
void TFMXDecoder::setPaulaVoice(ubyte v, PaulaVoice* p) {
    voiceVars[v].ch = p;
}
// rePlayer begin
void TFMXDecoder::setPath(std::string path, LoaderCallback loaderArg, void* loaderDataArg) {
    input.path = path;
    input.loader = loaderArg;
    input.loaderData = loaderDataArg;
// rePlayer end
    // Since most modules in these formats don't store a title/name internally,
    // this helper function constructs a name from the filename.
    name.clear();
    std::size_t found = path.find_last_of("/\\");
    if (found != std::string::npos) {
        std::string fname = path.substr(found+1);
        std::string mdatLC = "mdat.";
        std::string mdatUC = "MDAT.";
        if (fname.compare(0,mdatLC.length(),mdatLC) == 0 ||
            fname.compare(0,mdatUC.length(),mdatUC) == 0) {
            name = fname.substr( mdatLC.length() );
        }
        else {
            // Strip the filename extension.
            name = fname.substr(0, fname.rfind('.'));
        }
    }
}

// ----------------------------------------------------------------------

bool TFMXDecoder::init(void *data, udword length, int songNumber) {
    title.clear();
    author.clear();
    game.clear();
    duration = 0;

    if (data==0 || length==0 ) {  // re-init mode
        if (!admin.initialized) {
            return false;
        }
    }
    else {  // invalidate what has been found out before
        input.smplLoaded = false;
        input.mdatSize = input.smplSize = 0;

        if ( !detect(data,length) ) {
            return false;
        }
        // If we still have a sufficiently large buffer, reuse it.
        udword newLen = length;
        if (newLen > input.bufLen) {
            if (input.buf) {
                delete[] input.buf;
            }
            input.bufLen = 0;
            input.buf = new ubyte[newLen];
        }
        memcpy(input.buf,data,length);
        input.bufLen = newLen;
        input.len = length;
        
        // Set up smart pointer for unsigned input buffer access.
        pBuf.setBuffer(input.buf,input.bufLen);
    }

    // Check whether it's a single-file format.
    // If it is, this method defines various variables.
    if ( isMerged() || loadSamplesFile() ) {
        input.smplLoaded = true;
        // Make these the current ones.
        input.mdatSizeCurrent = input.mdatSize;
        input.smplSizeCurrent = input.smplSize;
    }
    else {
        // Can't proceed without samples data.
        return false;
    }

// --------
    
    // For convenience, since we don't relocate the module to offset 0.
    udword h = offsets.header;

    udword o1 = readBEudword(pBuf,h+0x1d0);  // offset to trackTable
    udword o2 = readBEudword(pBuf,h+0x1d4);  // offset to pattern offsets
    udword o3 = readBEudword(pBuf,h+0x1d8);  // offset to macro offsets
    if ( (o1|o2|o3) != 0 ) {
        variant.compressed = true;
    }
    else {
        o1 = 0x800;
        o2 = 0x400;
        o3 = 0x600;
    }
    // Check for out-of-bounds offsets. It may be the rare case of a TFMX PC
    // module conversion like "Tony & Friends in Kellogg's Land", which uses
    // little-endian offsets here.
    if (o1 >= input.bufLen || o2 >= input.bufLen || o3 >= input.bufLen) {
        // C++23 would have std::byteswap.
        o1 = byteSwap(o1);
        o2 = byteSwap(o2);
        o3 = byteSwap(o3);
    }
    offsets.trackTable = h+o1;
    offsets.patterns = h+o2;  // offset to array of offsets
    offsets.macros = h+o3;  // offset to array of offsets
    // If they are out-of-bounds, reject the module.
    if (offsets.trackTable >= input.bufLen || offsets.patterns >= input.bufLen || offsets.macros >= input.bufLen) {
        return false;
    }
    
    offsets.sampleData = h+input.mdatSize;
    offsets.silence = offsets.sampleData;
    // TFMX clears the first two words for one-shot samples.
    udword o = offsets.sampleData;
    pBuf[o] = pBuf[o+1] = pBuf[o+2] = pBuf[o+3] = 0;

    // Reject Atari ST TFMX files.
    bool foundHighMacroCmd = false;
    for (int m=0; m<7; m++) {
        udword macroOffs = getMacroOffset(m);
        udword macroEnd = getMacroOffset(m+1);
        if (macroEnd <= macroOffs) {
            break;
        }
        bool foundStop = false;
        while (macroOffs < macroEnd) {
            ubyte cmd = pBuf[macroOffs];
            if (cmd == 7) {
                foundStop = true;
                break;
            }
            if (cmd >= 0x40) {
                foundHighMacroCmd = true;
            }
            macroOffs += 4;
        };
        if (foundStop && foundHighMacroCmd) {
#if defined(DEBUG)
            cout << "WARNING: Rejecting Atari ST TFMX file!  " << input.path << endl;
#endif
            return false;
        }            
    }

    // Evaluate the compress identification fields at $0A and $0C.
    // In rare cases that part of the header has been overwritten
    // and is invalid.
    udword compressHint = readBEudword(pBuf,h+0x0c);
    if (compressHint!=0 && readBEuword(pBuf,h+0x0a)==1) {
        offsets.trackTableEnd = offsets.trackTable + compressHint - (0x800-0x10);
        if (offsets.trackTableEnd>getPattOffset(0)) {
            offsets.trackTableEnd = getPattOffset(0);
        }
    }
    else {
        offsets.trackTableEnd = getPattOffset(0);
    }

// ----------

    // Defaults only. Detection further below.
    setRate(50<<8);
    voices = 4;

    // Move these, if ever encapsulating sequencer.
    // So far, they have been constant, btw.
    sequencer.tracks = 8;
    sequencer.step.size = 16;

    for (size_t v=0; v<sizeof(channelToVoiceMap); v++) {
        channelToVoiceMap[v] = v&3;
    }

    variant.compressed = false;
    variant.finetuneUnscaled = false;
    variant.vibratoUnscaled = false;
    variant.vibratoTimeMask = false;
    variant.portaUnscaled = false;
    variant.portaOverride = false;
    variant.noNoteDetune = false;
    variant.setNoteV1 = false;
    variant.extraWaitV1 = false;
    variant.macroLoopExtraWait = false;
    variant.bpmSpeed5 = false;
    variant.noAddBeginCount = false;
    variant.noDelayedDMAon = false;
    variant.noTrackMute = false;
    variant.execOrder = MAC_MOD_SEQ;
    
    PattCmdFuncs[0] = &TFMXDecoder::pattCmd_End;
    PattCmdFuncs[1] = &TFMXDecoder::pattCmd_Loop;
    PattCmdFuncs[2] = &TFMXDecoder::pattCmd_Goto;
    PattCmdFuncs[3] = &TFMXDecoder::pattCmd_Wait;
    PattCmdFuncs[4] = &TFMXDecoder::pattCmd_Stop;
    PattCmdFuncs[5] = &TFMXDecoder::pattCmd_Note;
    PattCmdFuncs[6] = &TFMXDecoder::pattCmd_Note;
    PattCmdFuncs[7] = &TFMXDecoder::pattCmd_Note;
    PattCmdFuncs[8] = &TFMXDecoder::pattCmd_SaveAndGoto;
    PattCmdFuncs[9] = &TFMXDecoder::pattCmd_ReturnFromGoto;
    PattCmdFuncs[10] = &TFMXDecoder::pattCmd_Fade;
    PattCmdFuncs[11] = &TFMXDecoder::pattCmd_NOP;
    PattCmdFuncs[12] = &TFMXDecoder::pattCmd_Note;
    PattCmdFuncs[13] = &TFMXDecoder::pattCmd_NOP;
    PattCmdFuncs[14] = &TFMXDecoder::pattCmd_Stop;
    PattCmdFuncs[15] = &TFMXDecoder::pattCmd_NOP;

    TrackCmdFuncs[0] = &TFMXDecoder::trackCmd_Stop;
    TrackCmdFuncs[1] = &TFMXDecoder::trackCmd_Loop;
    TrackCmdFuncs[2] = &TFMXDecoder::trackCmd_Speed;
    // TIMESHARE command not needed.
    // We activate the 7VOICE command by default for more accurate
    // song duration detection.
    TrackCmdFuncs[3] = &TFMXDecoder::trackCmd_7V;
    TrackCmdFuncs[4] = &TFMXDecoder::trackCmd_Fade;

    // Start with mapping all undefined/unknown macro commands to NOP.
    for (ubyte m = 0; m<0x40; m++) {
        MacroDefs[m] = &macroDef_NOP;
    }
    MacroDefs[0] = &macroDef_StopSound;
    MacroDefs[1] = &macroDef_StartSample;
    MacroDefs[2] = &macroDef_SetBegin;
    MacroDefs[3] = &macroDef_SetLen;
    MacroDefs[4] = &macroDef_Wait;
    MacroDefs[5] = &macroDef_Loop;
    MacroDefs[6] = &macroDef_Cont;
    MacroDefs[7] = &macroDef_Stop;

    MacroDefs[8] = &macroDef_AddNote;
    MacroDefs[9] = &macroDef_SetNote;
    MacroDefs[0xa] = &macroDef_Reset;
    MacroDefs[0xb] = &macroDef_Portamento;
    MacroDefs[0xc] = &macroDef_Vibrato;
    MacroDefs[0xd] = &macroDef_AddVolNote;
    MacroDefs[0xe] = &macroDef_SetVolume;
    MacroDefs[0xf] = &macroDef_Envelope;

    MacroDefs[0x10] = &macroDef_LoopKeyUp;
    MacroDefs[0x11] = &macroDef_AddBegin;
    MacroDefs[0x12] = &macroDef_AddLen;
    MacroDefs[0x13] = &macroDef_StopSample;
    MacroDefs[0x14] = &macroDef_WaitKeyUp;
    MacroDefs[0x15] = &macroDef_Goto;
    MacroDefs[0x16] = &macroDef_Return;
    MacroDefs[0x17] = &macroDef_SetPeriod;
    
    MacroDefs[0x18] = &macroDef_SampleLoop;
    MacroDefs[0x19] = &macroDef_OneShot;
    MacroDefs[0x1a] = &macroDef_WaitOnDMA;
    MacroDefs[0x1b] = &macroDef_RandomPlay;
    MacroDefs[0x1c] = &macroDef_SplitKey;
    MacroDefs[0x1d] = &macroDef_SplitVolume;
    MacroDefs[0x1e] = &macroDef_RandomMask;
    MacroDefs[0x1f] = &macroDef_SetPrevNote;

    // Macro commands $1F AddChannel and $20 SubChannel are not implemented
    // since no file seems to use them. Also, number $1F is occupied by
    // SetPrevNote already. which would cause a conlict that would need
    // to be detected and prevent somehow.
    //
    // Macro command $20 Signal and its external write-only registers
    // as added by some TFMX variants is not needed either.
    MacroDefs[0x20] = &macroDef_UNDEF;
    
    MacroDefs[0x21] = &macroDef_PlayMacro;
    MacroDefs[0x22] = &macroDef_22;
    MacroDefs[0x23] = &macroDef_23;
    MacroDefs[0x24] = &macroDef_24;
    MacroDefs[0x25] = &macroDef_25;
    MacroDefs[0x26] = &macroDef_26;
    MacroDefs[0x27] = &macroDef_27;

    MacroDefs[0x28] = &macroDef_28;
    MacroDefs[0x29] = &macroDef_29;

    MacroDefs[0x30] = &macroDef_BranchIfSame;
    MacroDefs[0x31] = &macroDef_KeyUp;

    // TFMX v1.x up to and including v2.2 cannot be distinguished from
    // the later TFMX and/or variants. Unless the very rarely used old
    // header tag is used. And since the old-style effects (particularly
    // non-scaled vibrato and portamento) may be strictly required by
    // music created with v1/v2, a checksum based detection of specific
    // modules may be the only way. And the differences between v1 and v2
    // are of little use when detecting file contents.

    // TFMX v1.x
    if ( ((readBEudword(pBuf,offsets.header) == TFMX_HEX) &&  // old header
          (pBuf[offsets.header+4] == 0x20))
         || (input.versionHint == 1) ) {  // from TFHD
        setTFMXv1();
    }
    // aka TFMX Professional
    else {  // also  input.versionHint == 2   // from TFHD
        formatName = FORMAT_NAME_PRO;
    }
    // TFMX 7V is checked and set by adjustTraitsPost()

    // Last the rare checksum adjustments.
    traitsByChecksum();
    if (blacklisted) {
        return false;
    }

// ----------

    findSongs();
    // Some files contain SFX only and no valid song definitions.
    if (vSongs.size() == 0) {
        return false;
    }
    // If the file specifies a default start song (like TFMX-MOD does),
    // assume it's a single-song file format, and reduce number of songs to 1.
    if (input.startSongHint >= 0) {
        //cout << "MODSONG = " << input.startSongHint << endl;
        vSongs.clear();
        vSongs.push_back(input.startSongHint);
    }
    
    admin.startSong = (songNumber < 0) ? 0 : songNumber;
    if (admin.startSong > static_cast<int>(vSongs.size()-1)) {
        admin.startSong = 0;
    }
    restart();
    admin.initialized = true;

    // Determine duration with a dry-run till song-end.
    duration = 0;
    bool loopModeBak = loopMode;
    loopMode = false;
    do {
        duration += run();
    } while ( !songEnd && (duration<1000*60*59));
    loopMode = loopModeBak;
#if defined(DEBUG)
    cout << "Duration of " << input.path << "  #" << dec << admin.startSong << "  ";
    dumpTimestamp(duration);
    cout << endl;
#endif

    adjustTraitsPost();
    dumpModule();
    restart();
    return admin.initialized;
}

// With an initialized decoder, calling this should (re)start the
// decoder currently chosen song.
void TFMXDecoder::restart() {
    reset();

    uword so = vSongs[admin.startSong]<<1;
    sequencer.step.first = sequencer.step.current = readBEuword(pBuf,offsets.header+0x100+so);
    sequencer.step.last = readBEuword(pBuf,offsets.header+0x140+so);
    if ((admin.speed = readBEuword(pBuf,offsets.header+0x180+so)) >= 0x10) {
        setBPM(admin.speed);
        admin.speed = 0;
        if (variant.bpmSpeed5) {
            admin.speed = 5;
        }
    }
    admin.startSpeed = admin.speed;
    admin.count = 0;  // quick start
#if defined(DEBUG)
    cout << "Sequencer: " << tohex((uword)sequencer.step.first)
         << " to " << tohex((uword)sequencer.step.last)
         << " / speed " << (int)admin.speed << endl;
#endif
    softRestart();
}

// The bits that also are needed when restarting songs that end
// without looping.
void TFMXDecoder::softRestart() {
    songEnd = false;
    songPosCurrent = 0;
    tickFPadd = 0;
    triggerRestart = false;

    // Not all songs are designed for looping cleanly, so aid them.
    for (ubyte v=0; v<voices; v++) {
        VoiceVars& voice = voiceVars[v];
        voice.keyUp = true;
        voice.volume = voice.outputVolume = 0;
    }
    fade.active = false;
    fade.volume = fade.target = 64;
    fade.delta = 0;

    resetSequencer();
    processTrackStep();
}

// TFMX v2.2 is mostly like TFMX v1.x, but it adds macros 0x1a to 0x1e
// and enhances a few (like Addvol -> Addvol+note). There is no setTFMXv2()
// method, because it can be treated as a variant of v1.

void TFMXDecoder::setTFMXv1() {
    formatName = FORMAT_NAME;
    variant.noAddBeginCount = true;
    variant.vibratoUnscaled = true;
    variant.vibratoTimeMask = true;
    variant.finetuneUnscaled = true;
    variant.portaUnscaled = true;
    variant.portaOverride = true;
    variant.setNoteV1 = true;
    variant.extraWaitV1 = true;
    variant.noDelayedDMAon = true;
    variant.execOrder = SEQ_MOD_MAC;
    MacroDefs[0xd] = &macroDef_AddVolume;
    // max. macro cmd = $19
    for (ubyte m = 0x1a; m<0x40; m++) {
        MacroDefs[m] = &macroDef_NOP;
    }
}

void TFMXDecoder::adjustTraitsPost() {
    // If track command 7VOICE is used, activate 7V mode.
    // Unfortunately, the way TFMX assigns 16 virtual channels
    // to Amiga Paula using a logical AND 3, we cannot rely on
    // searching for the highest channel number.
    if (trackCmdUsed[3] ||
        input.versionHint==3 ) {  // from TFHD
        voices = 8;
        formatName = FORMAT_NAME_7V;
        // Hippel TFMX7V is 7 voices, mapping 3,4,5,6 to 3
        // Huelsbeck TFMX7V implements 8 voices, mapping 4,5,6,7 to 3
        channelToVoiceMap[3] = 7;
        channelToVoiceMap[4] = 3;
        channelToVoiceMap[5] = 4;
        channelToVoiceMap[6] = 5;
        channelToVoiceMap[7] = 6;
        TrackCmdFuncs[3] = &TFMXDecoder::trackCmd_7V;
    }
}

// ----------------------------------------------------------------------

// incomplete
void TFMXDecoder::dumpModule() {
#if defined(DEBUG)
    cout << getFormatName() << endl;
    cout << "Header at 0x" << tohex(offsets.header) << endl;
    cout << "Macro offsets at 0x" << tohex(offsets.macros) << endl;
    cout << "1st macro at 0x" << tohex(getMacroOffset(0)) << endl;
    cout << "Pattern offsets at 0x" << tohex(offsets.patterns) << endl;
    cout << "1st pattern at 0x" << tohex(getPattOffset(0)) << endl;
    cout << "Track table (sequencer) at 0x" << tohex(offsets.trackTable) << endl;
    cout << "Sample data at 0x" << tohex(offsets.sampleData) << endl;
    cout << "Songs: " << dec << getSongs() << endl;
    dumpMacros();

    cout << "Track Cmd used:" << endl
         << "01234" << endl;
    for (int m=0; m<=TRACK_CMD_MAX; m++) {
        char c = trackCmdUsed[m]? 'X' : ' ';
        cout << c;
    }
    cout << endl;

    cout << "Pattern Cmd used:" << endl
         << "0123456789abcdef" << endl;
    for (int m=0; m<16; m++) {
        char c = patternCmdUsed[m]? 'X' : ' ';
        cout << c;
    }
    cout << endl;

    // $00-$3f covers a larger range than defined by TFMX variants,
    // so we can apply AND $3f elsewhere and at the same time also handle
    // the few modules that specify a non-existant macro command like $31
    // (which we replace with the NOP macro command).
    cout << "Macro Cmd used:" << endl
         << "0000000000000000111111111111111122222222222222223333333333333333" << endl
         << "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef" << endl;
    for (int m=0; m<0x40; m++) {
        char c = macroCmdUsed[m]? 'X' : ' ';
        cout << c;
    }
    cout << endl;
#endif  // DEBUG
}

// ----------------------------------------------------------------------

// Cache the original pair of Paula start+length parameters,
// so it can be checked and adjusted in case of out of bounds
// access to sample data area.
void TFMXDecoder::toPaulaStart(VoiceVars& v, udword offset) {
    v.paulaOrig.offset = offset;
    v.ch->paula.start = makeSamplePtr( offset );
    takeNextBufChecked(v);
}

void TFMXDecoder::toPaulaLength(VoiceVars& v, uword length) {
    v.paulaOrig.length = length;
    v.ch->paula.length = length;
    takeNextBufChecked(v);
}

void TFMXDecoder::takeNextBufChecked(VoiceVars& v) {
    if (v.paulaOrig.offset >= input.len) {  // start outside sample data space
        v.ch->paula.start = makeSamplePtr( offsets.silence );
        v.ch->paula.length = 1;
    }
    // end outside sample data space
    else if ( (v.paulaOrig.offset+(v.paulaOrig.length<<1)) > input.len ) {
        v.ch->paula.length = (input.len - v.paulaOrig.offset)>>1;
        v.ch->paula.start = makeSamplePtr( v.paulaOrig.offset );
    }
    // an "else" case here (see DNSDecoder.cpp) is not needed by players
    // that always set start before length
    v.ch->takeNextBuf();
}

ubyte* TFMXDecoder::makeSamplePtr(udword offset) {
    return(pBuf.tellBegin() + offset);
}

// ----------------------------------------------------------------------

void TFMXDecoder::handleWaitOnPaulaDone() {
    // Pretend we have an interrupt handler that has evaluated
    // Paula "audio channel 0-3 block finished" interrupts meanwhile.
    for (ubyte v=0; v<voices; v++) {
        VoiceVars& voice = voiceVars[v];
        if (voice.waitOnDMACount >= 0) {  // 0 = wait once
            uword x = voice.ch->getLoopCount();
            uword y = voice.waitOnDMAPrevLoops;
            int d;
            if (x >= y) {
                d = x-y;
            }
            else {
                d = x+(0x10000-y);
            }
            if ( d > voice.waitOnDMACount ) {
                voice.waitOnDMACount = -1;
                if (voice.macro.state == 0) {
                    voice.macro.state = -1;
                }
            }
            else {
                voice.waitOnDMACount -= d;
                voice.waitOnDMAPrevLoops = voice.ch->getLoopCount();
            }
        }
    }  // voices
}

void TFMXDecoder::handleDelayedDMAoff() {
    for (ubyte v=0; v<voices; v++) {
        VoiceVars& voice = voiceVars[v];
        if (voice.macro.delayedOff) {
            voice.macro.delayedOff = false;
            voice.ch->off();
        }
    }
}

void TFMXDecoder::handleDelayedDMAon() {
    for (ubyte v=0; v<voices; v++) {
        VoiceVars& voice = voiceVars[v];
        if (voice.macro.delayedOn) {
            voice.macro.delayedOn = false;
            voice.ch->on();
        }
    }
}

void TFMXDecoder::runMain() {
    if (--admin.count < 0) {
        bool hasEnded = false;//rePlayer
        admin.count = admin.speed;  // reload
        int evalMaxLoops = RECURSE_LIMIT;
        do {
            sequencer.step.next = false;
            int countInactive = 0;
            int countInfinite = 0;
            for (ubyte t=0; t<sequencer.tracks; t++) {
                Track& tr = track[t];
                tr.on = getTrackMute(t);
                if (tr.PT >= 0x90) {
                    countInactive++;
                }
                else if (tr.pattern.infiniteLoop) {
                    countInfinite++;
                }
#if defined(DEBUG_RUN)
                cout << '[' << (int)t << "] ";
#endif
                processPTTR(tr);
#if defined(DEBUG_RUN)
                cout << endl;
#endif
                if (sequencer.step.next) {
                    break;
                }
            }  // next track
            // These are states where track sequencer cannot advance.
            if ( !sequencer.step.next ) {
                if ( (countInactive == sequencer.tracks) ||
                     (countInactive+countInfinite) == sequencer.tracks ) {
                    songEnd = true;
                    triggerRestart = true;
                }
            }
            // Loop on end?
            if (songEnd && loopMode) {
                songEnd = false;
                if (triggerRestart) {
                    softRestart();
                    processTrackStep();
                }
                hasEnded = true;// rePlayer
            }
        } while (sequencer.step.next && --evalMaxLoops>0 );
        songEnd |= hasEnded;//rePlayer
    }
}

void TFMXDecoder::processPTTR(Track& tr) {
    // PT < 0x80 : current pattern
    // PT >= 0x80 < 0x90 : continue pattern from previous step
    // PT >= 0x90 : track not used
    if (tr.PT < 0x90) {
        if (tr.pattern.offset == 0) {  // track didn't set valid PT before
            tr.PT = 0xff;
            return;
        }
        if (tr.pattern.wait == 0) {
            processPattern(tr);
        }
        else {
            tr.pattern.wait--;
#if defined(DEBUG_RUN)
            cout << "wait " << (int)tr.pattern.wait;
#endif
        }
    }
    else {
        if (tr.PT == 0xfe) {  // clear track
            tr.PT = 0xff;
            ubyte vNum = channelToVoiceMap[tr.TR & (sizeof(channelToVoiceMap)-1)];
            VoiceVars& v = voiceVars[vNum];
            v.macro.state = 0;
            v.ch->off();
        }
    }
}

int TFMXDecoder::run() {
    if (!admin.initialized) {
        return 0;
    }
#if defined(DEBUG_RUN)
    dumpTimestamp(songPosCurrent);
#endif
    playerCommon();
    tickFPadd += tickFP;
    int tick = tickFPadd>>8;
    tickFPadd &= 0xff;
    songPosCurrent += tick;
    return tick;
}

void TFMXDecoder::playerCommon() {
#if defined(DEBUG_RUN)
    cout << "  playerCommon()" << endl;
#endif
    if ( !songEnd || loopMode ) {
        handleWaitOnPaulaDone();
        handleDelayedDMAoff();

        if (variant.execOrder == MOD_MAC_SEQ) {
            for (ubyte v=0; v<voices; v++) {
                VoiceVars& voice = voiceVars[v];
                processModulation( voice );
                processMacroMain( voice );
                voice.ch->paula.period = voice.outputPeriod;
            }
            runMain();
        }
        else if (variant.execOrder == SEQ_MOD_MAC) {
            runMain();
            for (ubyte v=0; v<voices; v++) {
                VoiceVars& voice = voiceVars[v];
                processModulation( voice );
                processMacroMain( voice );
                voice.ch->paula.period = voice.outputPeriod;
            }
        }
        else {  // if (variant.execOrder == MAC_MOD_SEQ) {
            for (ubyte v=0; v<voices; v++) {
                VoiceVars& voice = voiceVars[v];
                processMacroMain( voice );
                processModulation( voice );
                voice.ch->paula.period = voice.outputPeriod;
            }
            runMain();
        }

        handleDelayedDMAon();
    }
}

void TFMXDecoder::noteCmd() {
    ubyte vNum = channelToVoiceMap[cmd.cd & (sizeof(channelToVoiceMap)-1)];
    VoiceVars& v = voiceVars[vNum];
#if defined(DEBUG_RUN)
    cout << "  ::noteCmd()  v" << tohex(v.voiceNum) << endl;
#endif
    if (cmd.aa == 0xfc) {  // lock note
    }
    else if (cmd.aa == 0xf7) {  // envelope
        v.envelope.speed = cmd.bb;
        ubyte tmp = (cmd.cd>>4)+1;
        v.envelope.count = v.envelope.flag = tmp;
        v.envelope.target = cmd.ee;
    }
    else if (cmd.aa == 0xf6) {  // vibrato
        // Unlike the variants of the vibrato macro command,
        // here the bitmask has never been dropped.
        ubyte tmp = cmd.bb&0xfe;
        v.vibrato.time = tmp;
        v.vibrato.count = tmp>>1;
        v.vibrato.intensity = cmd.ee;
        v.vibrato.delta = 0;
    }
    else if (cmd.aa == 0xf5) {  // key up
        v.keyUp = true;
    }
    else if (cmd.aa < 0xc0) {  // note
        if (variant.noNoteDetune) {
            v.detune = 0;
        }
        else {
            v.detune = (sbyte)cmd.ee;
        }
        v.noteVolume = (cmd.cd>>4);
        v.notePrevious = v.note;
        v.note = cmd.aa;
        v.keyUp = false;
        udword mo = getMacroOffset(cmd.bb & 0x7f);
        if (mo != v.macro.offset) {
            v.macro.branchIfSame = false;
        }
        v.macro.offset = mo;
        v.macro.state = 1;  // delayed macro init
    }
    else {  // cmd.aa >= $c0   portamento note
        v.portamento.count = cmd.bb;
        v.portamento.wait = 1;
        if (v.portamento.speed == 0) {
            v.portamento.period = v.period;
        }
        v.portamento.speed = cmd.ee;
        v.note = cmd.aa&0x3f;
        v.period = noteToPeriod(v.note);
    }
}

uword TFMXDecoder::noteToPeriod(int note) {
    if (note >= 0) {
        note &= 0x3f;
    }
    else if (note < -13) {
        note = -13;
    }
    return periods[note+13];
}

// ----------------------------------------------------------------------

bool TFMXDecoder::detect(void* data, udword len) {
#if defined(DEBUG)
    cout << "TFMXDecoder::detect()" << endl;
#endif
    ubyte *d = static_cast<ubyte*>(data);
    bool maybe = false;
    if ( (len >= 5) && !memcmp(d,TAG.c_str(),4) && d[4]==0x20 ) {
        maybe = true;
    }
    else if ( len >= TAG_TFMXSONG.length() && !memcmp(d,TAG_TFMXSONG.c_str(),TAG_TFMXSONG.length()) ) {
        maybe = true;
    }
    else if ( len >= TAG_TFMXSONG_LC.length() && !memcmp(d,TAG_TFMXSONG_LC.c_str(),TAG_TFMXSONG_LC.length()) ) {
        maybe = true;
    }
    else if ( len >= TAG_TFMXPAK.length() && !memcmp(d,TAG_TFMXPAK.c_str(),TAG_TFMXPAK.length()) ) {
        maybe = true;
    }
    else if ( len >= TAG_TFHD.length() && !memcmp(d,TAG_TFHD.c_str(),TAG_TFHD.length()) ) {
        maybe = true;
    }
    else if ( len >= TAG_TFMXMOD.length() && !memcmp(d,TAG_TFMXMOD.c_str(),TAG_TFMXMOD.length()) ) {
        maybe = true;
    }
    if (maybe) {
        offsets.header = 0;
        formatID = TAG;
        formatName = FORMAT_NAME_PRO;
        return true;
    }
    return false;
}

// ----------------------------------------------------------------------

const uword TFMXDecoder::periods[0xd+0x40] = {
    0x0d5c,
    // -0xf4 extra octave
    0x0c9c, 0x0be8, 0x0b3c, 0x0a9a, 0x0a02, 0x0a02, 0x0972,
    0x08ea, 0x086a, 0x07f2, 0x0780, 0x0718,
    // +0 standard octaves
    0x06ae, 0x064e, 0x05f4, 0x059e, 0x054d, 0x0501, 0x04b9, 0x0475,
    0x0435, 0x03f9, 0x03c0, 0x038c, 
    // +0x0c
    0x0358, 0x032a, 0x02fc, 0x02d0, 0x02a8, 0x0282, 0x025e, 0x023b, 
    0x021b, 0x01fd, 0x01e0, 0x01c6,
    // +0x18
    0x01ac, 0x0194, 0x017d, 0x0168, 0x0154, 0x0140, 0x012f, 0x011e,
    0x010e, 0x00fe, 0x00f0, 0x00e3, 
    // +0x24
    0x00d6, 0x00ca, 0x00bf, 0x00b4, 0x00aa, 0x00a0, 0x0097, 0x008f,
    0x0087, 0x007f, 0x0078, 0x0071,
    // +0x30
    0x00d6, 0x00ca, 0x00bf, 0x00b4, 0x00aa, 0x00a0, 0x0097, 0x008f,
    0x0087, 0x007f, 0x0078, 0x0071,
    // +0x3c
    0x00d6, 0x00ca, 0x00bf, 0x00b4
};

}  // namespace
