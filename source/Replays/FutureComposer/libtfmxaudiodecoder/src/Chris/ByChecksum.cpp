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
 
    // Danger Freak (1989) seems to be a special TFMX v1 variant.
    if (crc1 == 0x48960d8c || crc1 == 0x5dcd624f || crc1 == 0x3f0b151f) {
        setTFMXv1();
        variant.noNoteDetune = true;
        variant.portaUnscaled = false;
    }
    // Ooops Up by Peter Thierolf. First two sub-songs specify a BPM customization
    // that isn't compatible with the speed count value of default TFMX.
    else if (crc1 == 0x76f8aa6e) {
        variant.bpmSpeed5 = true;
    }
}
