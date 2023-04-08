#include "RePlayer.h"

#include <RePlayer/Core.h>

RePlayer* RePlayer::Create()
{
    return rePlayer::Core::Create();
}