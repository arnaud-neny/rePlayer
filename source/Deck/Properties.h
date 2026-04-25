#pragma once

#include <Core/Window.h>
#include <Replays/Replay.h>

namespace rePlayer
{
    using namespace core;

    class Player;

    class Properties : public Window
    {
    public:
        Properties();
        ~Properties();

        void Update(Player* player);

    private:
        std::string OnGetWindowTitle() override;
        void OnDisplay() override;

    private:
//         Serialized<Scale> m_scale = { "Scale", Scale::kFit };
        SmartPtr<Player> m_player;
    };
}
// namespace rePlayer