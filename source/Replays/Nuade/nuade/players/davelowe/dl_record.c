// SPDX-License-Identifier: MIT
// Dave Lowe player: the sample records of a module. A record is 14 bytes: the state word, the address and length (in
// words) of the sound, and, unless the state is Immediate (0), the address and length of its loop.

#include "dl_internal.h"

enum {
    RecordAt_State = 0,
    RecordAt_Address = 2,
    RecordAt_Length = 6,
    RecordAt_LoopAddress = 8,
    RecordAt_LoopLength = 12,
};

DlSampleRecord dl_read_sample_record(const DlImage* image, uint32_t record) {
    DlSampleRecord r = { .state = dli_u16(image, record + RecordAt_State) };
    r.address = dli_u32(image, record + RecordAt_Address);
    r.length_words = dli_u16(image, record + RecordAt_Length);
    if (r.state != DlSampleState_Immediate) {
        r.loop_address = dli_u32(image, record + RecordAt_LoopAddress);
        r.loop_length_words = dli_u16(image, record + RecordAt_LoopLength);
    }
    return r;
}

uint32_t dl_sample_record_of_address_field(uint32_t field) {
    return field - RecordAt_Address;
}

void dl_write_sample_address(DlImage* image, uint32_t record, uint32_t address) {
    dli_set32(image, record + RecordAt_Address, address);
}

void dl_write_sample_loop(DlImage* image, uint32_t record, uint32_t loop_address, uint16_t loop_length_words) {
    dli_set32(image, record + RecordAt_LoopAddress, loop_address);
    dli_set16(image, record + RecordAt_LoopLength, loop_length_words);
}
