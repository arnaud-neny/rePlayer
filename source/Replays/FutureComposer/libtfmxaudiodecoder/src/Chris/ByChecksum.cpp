// Since the TFMX header and data don't tell which specific version or
// variant of TFMX are required, this function comes as a last resort.
// It tries to recognize specific files via a checksum of a small portion
// of the pattern data and then adjusts player traits.

#include "TFMXDecoder.h"
#include "CRCLight.h"

#include <set>

using namespace tfmxaudiodecoder;

void tfmxaudiodecoder::TFMXDecoder::traitsByChecksum() {
    CRCLight crc;
    smartPtr<const ubyte> sBuf(pBuf.tellBegin(),pBuf.tellLength());
    udword p0 = offsets.header + readBEudword(sBuf,offsets.patterns);
    udword crc1 = crc.get(sBuf,p0,0x100);
#if defined(DEBUG)
    cout << "CRC = " << tohex(crc1) << "  " << input.path << endl;
#endif

    // Gem'X. No checksum based adjustments required, but the soundtrack
    // strictly requires a special variant of $00 DMAoff and $01 DMAon
    // as well as the set of macro commands $22 to $29. The original machine
    // code player seems to be unique and does funky (most likely experimental)
    // things with its combination of variants of the macros $00 and $01.
    // Such as optionally controlling delayed channel on after waiting for
    // several vertical raster lines or setting Paula period 4 on channel off
    // (with the result being hardware dependent and uncertain).
    // Macro commands used:
    // 000000000000000011111111111111112222222222222222
    // 0123456789abcdef0123456789abcdef0123456789abcdef
    // XXXXX XXXX  XX X    X   XXX       XXXXXXXX      

    // Turrican
    std::set<udword> T1_checksums = {
        0x73868fda,  // title, credits, highscore
        0x93730029,  // title (old rips from 1990)
        0x6e799869,  // world 1
        0x3d00fc52,  // world 2
        0xd76d33ed,  // world 3
        0xb989d570,  // world 4
        0x8fa80b4e,  // world 5
        0x88f7c34b,  // loader
        0xb762f2dc   // bonus
    };
    // Turrican II (excl. loader jingle!)
    std::set<udword> T2_checksums = {
        0xe2d6b5e0,  // world 1
        0x0bf41b64,  // world 2
        0x19ac72c3,  // world 3
        0x03854473,  // world 4
        0x9430d9de,  // world 5
        0xcbb842b0,  // loader (original "unfixed", not "fixed" version!)
        0x78cd70f9   // demo
    };
    // Turrican II (1991) ingame music requires a special player variant
    // with different execution order of macros and effects.
    if (T2_checksums.count(crc1) >= 1) {
        variant.execOrder = MOD_MAC_SEQ;
    }
    // Turrican (1990) is a TFMX v1/v2 variant and strictly requires old
    // features such as non-scaled vibrato/portamento.
    else if (T1_checksums.count(crc1) >= 1) {
        setTFMXv1();
        variant.vibratoTimeMask = false;
        MacroDefs[0x0d] = &macroDef_AddVolNote;
        MacroDefs[0x1a] = &macroDef_WaitOnDMA;
        MacroDefs[0x1c] = &macroDef_SplitKey;
        MacroDefs[0x1d] = &macroDef_SplitVolume;
    }
    // Turrican 3 level 5 / world 5
    else if (crc1 == 0xc7ae8de6) {
        if ( readBEudword(pBuf,4+getMacroOffset(0x4c)) != 0x02008c0e ) {
            blacklisted = true;
        }
    }
    // Danger Freak (1989) seems to be a special TFMX v1 variant.
    else if (crc1 == 0x48960d8c || crc1 == 0x5dcd624f || crc1 == 0x3f0b151f) {
        setTFMXv1();
        variant.noNoteDetune = true;
    }
    // Hard'n'Heavy (1989) is a TFMX v1 variant. In particular, it strictly
    // requires the old AddBegin macro that is missing the effect updates.
    // Alternatively, the AddBegin count parameter must be ignored.
    // The game's title soundtrack sounds wrong in many videos.
    else if (crc1 == 0x27f8998c || crc1 == 0x26447707 ||
             // somebody compressed these files, they are not the originals!
             crc1 == 0xd404651b || crc1 == 0xb5348633 ) {
        setTFMXv1();
     }
    // R-Type (1989).
    else if (crc1 == 0x8ac70fc8) {
        setTFMXv1();
        variant.macroLoopExtraWait = true;
        // If it's the bad rip where the first 192 samples are missing,
        // blacklist it. See README_BAD.md file. Checking for minimum
        // sample file size 116160 would work, too.
        if (0xe8ff20f9 != readBEudword(sBuf,offsets.sampleData+4) ||
            0xe8f700fd != readBEudword(sBuf,offsets.sampleData+0xbc) ) {
            blacklisted = true;
        }
    }
    // The Adventures of Quik & Silva.
    else if (crc1 == 0x04f469a6 || crc1 == 0xd37c9008 ) {
        variant.execOrder = MAC_MOD_SEQ;
    }
    // Rock'n'Roll (1989). No checksum based adjustments required, because
    // it uses the unique header tag that was specific to TFMX before v1.
    // Except for the intro.
    else if (crc1 == 0xda279570) {  // intro
        setTFMXv1();
    }
    // Particularly non-scaled vibrato is required by these old TFMX v1
    // modules. There may be a few more that could be set to v1, e.g.
    // Wanted Team's EP_TFMX.lha mentions a list, but whether it would
    // be audible remains to be investigated.
    //
    // Grand Monster Slam
    else if (crc1 == 0xb54457fc || crc1 == 0x97707404 ||
             // Circus Attractions
             crc1 == 0x5f04d9af || crc1 == 0x72ef7307 ||
             // Gordian Tomb
             crc1 == 0xf2292eae ||
             // Oxxonian
             crc1 == 0x0629665d ||
             // X-Out
             crc1 == 0x9264f036 ||  // title
             crc1 == 0x22a86efb ||  // level 1
             crc1 == 0x711d8520 ||  // level 2
             crc1 == 0x2f525ee0 ||  // level 3
             crc1 == 0x8412d8d5 ||  // level 4
             crc1 == 0xc0376a19 ||  // level 5
             crc1 == 0xade39424 ||  // end
             crc1 == 0x2adbf5f9 ||  // high-scores
             crc1 == 0x236d305d ||  // load
             crc1 == 0x14adce8c) {  // shop (?)
        setTFMXv1();
    }
    // Z-Out uses an unusual player variant newer than TFMX v2.2, which
    // uses scaled vibrato/portamento already but not delayed channel on yet.
    // The macro scripts of some instruments strictly require immediate
    // channel on, or else sounds will stay silent.
    //
    // Alternative versions of the Z-Out soundtrack are not affected,
    // and their checksums differ.
    //
    // This also fixes an uneven sample start address, which isn't illegal,
    // but an accident that causes audible side-effects. Also is the proper
    // fix for the Wanted Team's rip "Z-Out 5" where the sample file would be
    // one byte too short for the sample parameters in this macro.
    else if (crc1 == 0x8648904b ||  // title
             crc1 == 0xb7f700b2 || crc1 == 0x0eabbc61 ||  // ingame
             crc1 == 0x999c8c6b || crc1 == 0xdb5e4afc ||  // ingame
             crc1 == 0x9d652521 || crc1 == 0xa1dc3139) {  // ingame
        variant.noDelayedDMAon = true;  // strictly required!
        // Repair one instrument. The preceding sample area (in macro 2)
        // is from 0x789a to 0x884e, and there's no indication that it's
        // not an off-by-one bug in macro 10.
        udword mo = getMacroOffset(0x10);
        if (readBEudword(pBuf,mo+4) == 0x0200884f) {
            pBuf[mo+4+3] = 0x4e;  // set begin to 0x884e
        }
    }
    // Software Manager - Titel 2
    else if (crc1 == 0xa8566760) {
        variant.noTrackMute = true;
    }
    // BiFi Adventure 2 - Ongame
    else if (crc1 == 0xab1a6c6e) {
        variant.noTrackMute = true;
    }
    // Ooops Up by Peter Thierolf. First two sub-songs specify a BPM customization
    // that isn't compatible with the speed count value of default TFMX.
    else if (crc1 == 0x76f8aa6e) {
        variant.bpmSpeed5 = true;
    }
    // TFMX music by ern0 in the music demo "Musication Vol. 1" are
    // replayed with a TFMX v1 player.
    else if (crc1 == 0x0eed9c91 ||  // Danubius Replay (aka Gitar)
             crc1 == 0x1a5d2b53 ||  // Flying World
             crc1 == 0x22a92c26 ||  // Armageddon
             crc1 == 0xe60babf2     // Magnetic Fields IV (aka Oxygen)
             ) {
        setTFMXv1();
        // Without summing up the findings in ticket #16 (like questionable
        // start/end points), the three main guitar samples are shifted
        // into only the positive side of value range of signed samples.
        // That causes clicks also because it leads to three defective,
        // negative peak values sticking out in each of them. It could be
        // that with real Amiga hardware, those samples are less of an issue.
        // The following centers the samples properly around zero,
        // which reduces the primary problem with them.
        if (crc1 == 0x0eed9c91) {  // Danubius Replay (aka Gitar)
            for (int i=4; i<0x303e4; ++i) {
                pBuf[offsets.sampleData+i] -= 0x38;
            }
        }
    }
    else if (crc1 == 0x5fb2f54e) {  // Puzzy
        setTFMXv1();
        // Fix second song's track end.
        pBuf[offsets.header+0x143] = 0x6f;
    }
    else if (crc1 == 0xcb3c7113) {  // Sledge Hammer One (aka Hammer One)
        setTFMXv1();
        // Fix song start/end. Song table is wrecked.
        pBuf[offsets.header+0x101] = 0x04;
        pBuf[offsets.header+0x141] = 0x6b;
        // Invalidate track 0 of step 0 as to avoid subsongs starting at 0.
        pBuf[offsets.trackTable] = 0xff;
    }
    // File "mdat.blade of destiny - titel (7ch)" is bad/corrupted.
    else if (crc1 == 0xc83b701b) {
        blacklisted = true;
    }
    // Bundesliga Manager HATTRICK features only a single title theme
    // using 7V mode. The second song is a fragment of the title theme
    // but without proper track commands to initialize 7V mode.
    else if (crc1 == 0x3c2eb450) {
        int song = 1;
        // Set start step to 0x1ff to make song invalid.
        writeBEword(pBuf,offsets.header+0x100+(song<<1),0x1ff);
    }
}
