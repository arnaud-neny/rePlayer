#pragma once

#include <Containers/HashMap.h>
#include <Core/Window.h>

#include <Database/Types/SubsongID.h>
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

        static uint32_t GetCRC(const char* str);

    private:
        SmartPtr<Player> m_player;
        HashMap<MediaType, uint32_t> m_selectedTabs;

        SubsongID m_currentSubsongId;
    };
}
// namespace rePlayer