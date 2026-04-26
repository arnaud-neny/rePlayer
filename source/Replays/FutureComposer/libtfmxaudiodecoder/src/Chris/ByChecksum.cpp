// Since the TFMX header and data don't tell which specific version or
// variant of TFMX are required, this function comes as a last resort.
// It tries to recognize specific files via a checksum of a small portion
// of the pattern data and then adjusts player traits.

#include "TFMXDecoder.h"
#include "CRCLight.h"

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
    // strictly requires a special variant of $00 DMAoff as well as the
    // set of macro commands $22 to $29. Macro commands used:
    // 000000000000000011111111111111112222222222222222
    // 0123456789abcdef0123456789abcdef0123456789abcdef
    // XXXXX XXXX  XX X    X   XXX       XXXXXXXX      

    // Rock'n'Roll (1989). No checksum based adjustments required, because
    // it uses the unique header tag that was specific to TFMX before v1.

    // Danger Freak (1989) seems to be a special TFMX v1 variant.
    if (crc1 == 0x48960d8c || crc1 == 0x5dcd624f || crc1 == 0x3f0b151f) {
        setTFMXv1();
        variant.noNoteDetune = true;
        variant.portaUnscaled = false;
    }
    // Hard'n'Heavy (1989) is a TFMX v1 variant with an AddBegin macro
    // that is missing the effect updates. The game soundtrack sounds wrong
    // in many videos.
    else if (crc1 == 0x27f8998c || crc1 == 0x26447707 ||
             // somebody compressed these files, they are not the originals!
             crc1 == 0xd404651b || crc1 == 0xb5348633 ) {
        setTFMXv1();
        variant.portaUnscaled = true;
    }
    // R-Type (1989).
    else if (crc1 == 0x8ac70fc8) {
        setTFMXv1();
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
}
