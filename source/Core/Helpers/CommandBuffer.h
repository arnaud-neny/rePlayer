#pragma once

#include <Containers/Array.h>

namespace core
{
    class CommandBuffer
    {
    public:
        struct Command
        {
            union
            {
                struct
                {
                    uint16_t numEntries;
                    uint16_t commandId;
                };
                uint32_t command = 0;
            };

            bool operator!=(const Command& other) const;
        };
        template <typename CommandType, auto newCommandId, auto rangeTypeMin, auto rangeTypeMax>
        struct CommandExclusive : public Command
        {
            static constexpr uint16_t kCommandId = uint16_t(newCommandId);
            static constexpr uint16_t kRangeMin = uint16_t(rangeTypeMin);
            static constexpr uint16_t kRangeMax = uint16_t(rangeTypeMax);
            CommandExclusive();
        };

    public:
        CommandBuffer(Array<Command>& commands);

        template <typename CommandType>
        CommandType* Find(CommandType* defaultCommand = nullptr) const;

        template <typename CommandType>
        void Add(const CommandType* command);

        template <typename CommandType>
        CommandType* Create(size_t commandSize = sizeof(CommandType));

        template <typename CommandType>
        void Update(const CommandType* command, bool isRemoved);

        void Remove(uint16_t commandId);

    private:
        Array<Command>& m_commands;
    };
}
// namespace rePlayer