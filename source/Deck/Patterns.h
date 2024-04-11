#pragma once

#include <Core/Window.h>
#include <Replays/Replay.h>

namespace rePlayer
{
    using namespace core;

    class Player;

    class Patterns : public Window
    {
    public:
        Patterns();
        ~Patterns();

        void Update(Player* player);

    private:
        std::string OnGetWindowTitle() override;
        void OnDisplay() override;

    private:
        static constexpr uint32_t kCharWidth = 4;
        static constexpr uint32_t kCharHeight = 6;

    private:
        Replay::Patterns m_patterns;
        uint32_t m_numLines = 45;
        Serialized<Replay::Patterns::Flags> m_flags = { "Flags", Replay::Patterns::Flags(Replay::Patterns::kDisplayAll) };
    };
}
// namespace rePlayer