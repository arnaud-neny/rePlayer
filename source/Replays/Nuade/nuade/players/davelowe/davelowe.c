// SPDX-License-Identifier: MIT
// Dave Lowe player: module kinds and lifecycle. The engine itself is in dl_engine.c.

#include "dl_internal.h"
#include <stdlib.h>
#include <string.h>

// The built-in tables, in the bytes after the module
static const uint8_t s_builtin_tables[] = {
    0x00, 0x00, 0x00, 0x00, // empty list
    0x00, 0x08, 0x00, 0x00, // idle sequence: the command that takes the next pattern
    0x00, 0x00, 0x00, 0xff, // silent envelope: volume 0, then the end mark
};

DlImage* dl_image_create(const uint8_t* data, size_t size) {
    if (size == 0 || size > 0x1000000) {
        return nullptr;
    }
    DlImage* image = calloc(1, sizeof(*image));
    // the built-in tables sit behind a Paula tail of zeros, so a sample that runs off the module end reads zeros
    const uint32_t builtin_at = (uint32_t)((size + 1) & ~(size_t)1) + NUADE_PAULA_MAX_SAMPLE_BYTES;
    const uint32_t total = builtin_at + DL_BUILTIN_SIZE;
    uint8_t* bytes = nuade_paula_padded_copy(nullptr, total);
    if (image == nullptr || bytes == nullptr) {
        free(image);
        free(bytes);
        return nullptr;
    }
    memcpy(bytes, data, size);
    memcpy(bytes + builtin_at, s_builtin_tables, sizeof(s_builtin_tables));
    image->data = bytes;
    image->module_size = (uint32_t)size;
    image->size = total;
    image->builtin
        = (DlBuiltin) { .empty_list = builtin_at, .idle_sequence = builtin_at + 4, .silent_envelope = builtin_at + 8 };
    image->builtin_free = builtin_at + (uint32_t)sizeof(s_builtin_tables);
    return image;
}

bool dl_image_keep_original(DlImage* image) {
    image->original = malloc(image->module_size);
    if (image->original == nullptr) {
        return false;
    }
    memcpy(image->original, image->data, image->module_size);
    return true;
}

void dl_image_restore(DlImage* image) {
    if (image->original != nullptr) {
        memcpy(image->data, image->original, image->module_size);
    }
}

void dl_image_destroy(DlImage* image) {
    if (image != nullptr) {
        free(image->original);
        free(image->data);
        free(image);
    }
}

// The code hunk of an executable module as an image of its own
static DlImage* hunk_image(const uint8_t* data, uint32_t code_size) {
    return dl_image_create(data + DL_HUNK_CODE_OFFSET, code_size);
}

static DlImage* open_game(const DlImage* file, const uint8_t* data, DlPlayer* player) {
    uint32_t code_size;
    if (!dl_game_extract(file, &code_size)) {
        return nullptr;
    }
    DlImage* code = hunk_image(data, code_size);
    if (code == nullptr || !dl_game_locate(code, &player->game)) {
        dl_image_destroy(code);
        return nullptr;
    }
    player->kind = DlKind_Game;
    player->variant = player->game.variant;
    player->subsong_count = player->game.subsong_count;
    player->first_sample_record = player->game.sample_records;
    return code;
}

static DlImage* open_deli(const DlImage* file, const uint8_t* data, DlPlayer* player) {
    uint32_t code_size;
    if (!dl_deli_extract(file, &code_size)) {
        return nullptr;
    }
    DlImage* code = hunk_image(data, code_size);
    if (code == nullptr || !dl_deli_locate(code, &player->deli)) {
        dl_image_destroy(code);
        return nullptr;
    }
    player->kind = DlKind_Deli;
    player->variant = dl_deli_variant();
    player->subsong_count = player->deli.subsong_count;
    player->first_sample_record = DL_NOT_FOUND;
    return code;
}

// The file itself is the image of a ".dln" module
static DlImage* open_new(DlImage* file, DlPlayer* player) {
    if (!dl_new_locate(file, &player->new_module)) {
        return nullptr;
    }
    player->kind = DlKind_New;
    player->variant = dl_new_variant(&player->new_module);
    player->subsong_count = player->new_module.subsong_count;
    player->first_sample_record = player->new_module.first_sample_record;
    return file;
}

// The module as the engine sees it: the code hunk of a game driver or a Deli module, or the whole ".dln" file. Null if
// the data is not a Dave Lowe module.
static DlImage* open_module(const uint8_t* data, size_t size, DlPlayer* player) {
    DlImage* file = dl_image_create(data, size);
    if (file == nullptr) {
        return nullptr;
    }
    DlImage* image = open_game(file, data, player);
    if (image == nullptr) {
        image = open_deli(file, data, player);
    }
    if (image == nullptr) {
        image = open_new(file, player);
    }
    if (image != file) {
        dl_image_destroy(file);
    }
    if (image != nullptr && !dl_image_keep_original(image)) {
        dl_image_destroy(image);
        return nullptr;
    }
    return image;
}

bool dl_probe(const uint8_t* data, size_t size) {
    DlPlayer player = { 0 };
    DlImage* image = open_module(data, size, &player);
    dl_image_destroy(image);
    return image != nullptr;
}

DlPlayer* dl_create(const uint8_t* data, size_t size) {
    DlPlayer* player = calloc(1, sizeof(*player));
    if (player == nullptr) {
        return nullptr;
    }
    player->image = open_module(data, size, player);
    if (player->image == nullptr) {
        free(player);
        return nullptr;
    }
    return player;
}

void dl_destroy(DlPlayer* player) {
    if (player != nullptr) {
        dl_image_destroy(player->image);
        free(player);
    }
}

uint32_t dl_subsong_count(const DlPlayer* player) {
    return player->subsong_count;
}

const char* dl_kind_name(const DlPlayer* player) {
    if (player->kind == DlKind_Game) {
        return "game driver";
    }
    if (player->kind == DlKind_Deli) {
        return "Deli module";
    }
    return player->new_module.engine_a ? "new module, first engine" : "new module, second engine";
}

bool dl_start(DlPlayer* player, uint32_t subsong) {
    if (subsong >= player->subsong_count) {
        return false;
    }
    DlSubsong data;
    switch (player->kind) {
        case DlKind_Game:
            data = dl_game_subsong(player->image, &player->game, subsong);
            break;
        case DlKind_Deli:
            data = dl_deli_subsong(player->image, &player->deli, subsong);
            break;
        case DlKind_New:
            data = dl_new_subsong(player->image, &player->new_module, subsong);
            break;
    }
    dl_engine_start(&player->engine, player->image, &player->variant, &data);
    return true;
}

DlTickResult dl_tick(DlPlayer* player, NuadeVoices* voices) {
    dl_engine_tick(&player->engine);
    dl_engine_sync(&player->engine, voices);
    return (DlTickResult) { .looped = player->engine.loop.looped };
}

DlVoiceState dl_voice_state(const DlPlayer* player, int voice) {
    if (voice < 0 || voice >= DL_VOICES) {
        return (DlVoiceState) { .note = -1, .instrument = -1 };
    }
    const DlVoice* v = &player->engine.voices[voice];
    int instrument = -1;
    if (v->sample_record >= player->first_sample_record) {
        const uint32_t offset = v->sample_record - player->first_sample_record;
        instrument = offset % DL_SAMPLE_RECORD_SIZE == 0 ? (int)(offset / DL_SAMPLE_RECORD_SIZE) : -1;
    }
    return (DlVoiceState) { .note = v->note, .instrument = instrument };
}
