// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#include "Dump.h"
#include "FC.h"

#include <iostream>
#include <iomanip>
using namespace std;

void dumpLines(smartPtr<ubyte>& fcBuf, udword startOffset, sdword length, int blockLen, int index) {
    int num = 0;
    while (length > 0) {
        cout << "(0x" << hex << setw(2) << setfill('0');
        if (index>=0)
            cout << index;
        else
            cout << num;
        cout << "): ";
        num++;
        
        int b = 0;
        for (int i=0; i<blockLen; i++) {
            cout << hex << setw(2) << setfill('0')
                << (int)fcBuf[startOffset++] << " ";
            b++;
            if (b==16) {
                cout << endl;
                cout << "        ";
                b = 0;
            }
        }
        cout << endl;
        length -= blockLen;
    };
}

void dumpBlocks(smartPtr<ubyte>& fcBuf, udword startOffset, sdword length, int blockLen) {
    int num = 0;
    while (length > 0) {
        cout << "(0x" << hex << setw(2) << setfill('0') << num << "):" << endl;
        num++;
        
        int b = 0;
        for (int i=0; i<blockLen; i++) {
            cout << hex << setw(2) << setfill('0')
                << (int)fcBuf[startOffset++] << " ";
            b++;
            if (b==16) {
                cout << endl;
                b = 0;
            }
        }
        cout << endl;
        length -= blockLen;
    };
    cout << endl;
}

void dumpByte(int b) {
    cout << hex << setw(2) << setfill('0') << (b&0xff) << ' ';
}

void dumpTimestamp(udword ms) {
    udword secs = ms / 1000;
    ms %= 1000;
    udword mins = secs / 60;
    secs %= 60;
    cout << dec << setw(2) << setfill('0') << mins << ':'
         << setw(2) << setfill('0') << secs;
}

// --------------------------------------------------------------------------

void FC::dumpModule() {
    cout << getFormatName() << endl;
    cout << "Header at 0x" << hex << offsets.header << endl;
    cout << "Sample headers at 0x" << hex << offsets.sampleHeaders << endl;
    cout << "Sample data at 0x" << hex << offsets.sampleData << endl;
    cout << "Songs: " << dec << (int)stats.songs << endl;

    cout << "Volume modulation sequences: 0x" << hex << setfill('0') << stats.volSeqs
         << " at 0x" << hex << setw(8) << offsets.volModSeqs << endl;
    if ( !traits.compressed) {
        dumpBlocks(fcBuf,offsets.volModSeqs,stats.volSeqs*64,64);
    }
    else {
        for (int i=0; i<stats.volSeqs; i++) {
            udword o = getVolSeq(i);
            udword len = getVolSeqEnd(i)-o;
            dumpLines(fcBuf,o,len,len,i);
            if (len<64)
                cout << endl;
        }
    }

    cout << "Sound modulation sequences: 0x" << hex << setfill('0') << stats.sndSeqs
         << " at 0x" << hex << setw(8) << offsets.sndModSeqs << endl;
    if ( !traits.compressed) {
        dumpBlocks(fcBuf,offsets.sndModSeqs,stats.sndSeqs*64,64);
    }
    else {
        for (int i=0; i<stats.sndSeqs; i++) {
            udword o = getSndSeq(i);
            udword len = getSndSeqEnd(i)-o;
            dumpLines(fcBuf,o,len,len,i);
            if (len<64)
                cout << endl;
        }
    }

    cout << "Track table (sequencer): at " << hex << setw(8) << setfill('0') << offsets.trackTable << endl;
    dumpLines(fcBuf,offsets.trackTable,trackTabLen,trackStepLen);
    cout << endl;

    cout << "Patterns: 0x" << hex << stats.patterns << " at 0x" << offsets.patterns << endl;
    if ( !traits.compressed ) {
        dumpBlocks(fcBuf,offsets.patterns,
                   stats.patterns*traits.patternSize,traits.patternSize);
    }
    else {
        for (int p=0; p<stats.patterns; p++) {
            uword po = readBEuword(fcBuf,offsets.patterns+(p<<1));
            dumpLines(fcBuf,po,traits.patternSize,traits.patternSize,p);
        }
    }

    cout << "Samples/Waveforms: 0x" << hex << stats.samples << endl;
    for (int sam = 0; sam < stats.samples; sam++) {
        cout
            << "0x" << hex << setw(2) << sam << ": "
            << hex << setw(8) << setfill('0') << samples[sam].startOffs << " "
            << hex << setw(4) << setfill('0') << samples[sam].len << " "
            << hex << setw(4) << setfill('0') << samples[sam].repOffs << " "
            << hex << setw(4) << setfill('0') << samples[sam].repLen << " "
            << endl;
    }
}
