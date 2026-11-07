// Registry lookups over the generated player list.

#include <nuade/nuade.h>

#include "nuade_base.h"
#include "nuade_registry.h"
#include <string.h>

uint32_t nuade_player_count(void) {
    return nuade_registry_player_count;
}

const NuadePlayer* nuade_player_get(uint32_t index) {
    return index < nuade_registry_player_count ? nuade_registry_players[index] : nullptr;
}

const NuadePlayer* nuade_player_find(const char* id) {
    if (id == nullptr) {
        return nullptr;
    }
    for (uint32_t i = 0; i < nuade_registry_player_count; i++) {
        if (strcmp(nuade_registry_players[i]->id, id) == 0) {
            return nuade_registry_players[i];
        }
    }
    return nullptr;
}
