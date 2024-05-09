#pragma once

#include "CommandBuffer.h"

namespace core
{
    inline bool CommandBuffer::Command::operator!=(const Command& other) const
    {
        return command != other.command;
    }

    template <typename CommandType, auto newCommandId, auto rangeTypeMin, auto rangeTypeMax>
    inline CommandBuffer::CommandExclusive<CommandType, newCommandId, rangeTypeMin, rangeTypeMax>::CommandExclusive()
        :
        Command{ uint16_t((sizeof(CommandType) + sizeof(Command) - 1) / sizeof(Command)) - 1, kCommandId }
    {}

    inline CommandBuffer::CommandBuffer(Array<Command>& commands)
        : m_commands(commands)
    {}

    template <typename CommandType>
    inline CommandType* CommandBuffer::Find(CommandType* defaultCommand) const
    {
        for (auto commands = m_commands.Items(), e = commands + m_commands.NumItems(); commands < e; commands += commands->numEntries + 1)
        {
            if (commands->commandId == CommandType::kCommandId)
                return reinterpret_cast<CommandType*>(commands);
        }
        return defaultCommand;
    }

    template <typename CommandType>
    inline void CommandBuffer::Add(const CommandType* command)
    {
        assert(command < m_commands.begin() || command >= m_commands.end());
        for (uint32_t i = 0; i < m_commands.NumItems();)
        {
            if (m_commands[i].commandId >= CommandType::kRangeMin && m_commands[i].commandId <= CommandType::kRangeMax)
                m_commands.RemoveAt(i, m_commands[i].numEntries + 1);
            else
                i += m_commands[i].numEntries + 1;
        }
        m_commands.Add(command, command->numEntries + 1);
    }

    template <typename CommandType, typename... Args>
    inline CommandType* CommandBuffer::Create(size_t commandSize, Args&&... args)
    {
        for (uint32_t i = 0; i < m_commands.NumItems();)
        {
            if (m_commands[i].commandId >= CommandType::kRangeMin && m_commands[i].commandId <= CommandType::kRangeMax)
                m_commands.RemoveAt(i, m_commands[i].numEntries + 1);
            else
                i += m_commands[i].numEntries + 1;
        }
        auto numItems = uint16_t((commandSize + sizeof(Command) - 1) / sizeof(Command));
        auto command = new (m_commands.Push(numItems)) CommandType(std::forward<Args>(args)...);
        command->numEntries = numItems - 1;
        return command;
    }

    template <typename CommandType>
    inline void CommandBuffer::Update(const CommandType* command, bool isRemoved)
    {
        if (command >= m_commands.begin() && command < m_commands.end())
        {
            if (isRemoved)
                m_commands.RemoveAt(uint32_t(reinterpret_cast<const Command*>(command) - m_commands.Items()), command->numEntries + 1);
        }
        else if (!isRemoved)
            Add(command);
        else
            Remove(command->commandId);
    }

    inline void CommandBuffer::Remove(uint16_t commandId)
    {
        for (uint32_t i = 0; i < m_commands.NumItems();)
        {
            if (m_commands[i].commandId == commandId)
                m_commands.RemoveAt(i, m_commands[i].numEntries + 1);
            else
                i += m_commands[i].numEntries + 1;
        }
    }
}
// namespace rePlayer