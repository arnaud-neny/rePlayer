#include <Core/String.h>
#include <ImGui.h>
#include <ImGui/imgui_internal.h>

#include "BusySpinner.h"

#include <atomic>

namespace rePlayer
{
    BusySpinner::BusySpinner(uint32_t color)
        : m_color(color)
    {}

    BusySpinner::~BusySpinner()
    {
        for (auto* message = m_messages.root; message;)
        {
            auto* next = message->next;
            delete message;
            message = next;
        }
        for (auto* stringCache = m_messages.stringCache; stringCache;)
        {
            auto* next = stringCache->next;
            delete stringCache;
            stringCache = next;
        }
    }

    void BusySpinner::Update()
    {
        // ugly check on nullptr but it's fine as long as it's not virtual (it's just to handle the smart ptr with nullptr entry)
        if (this != nullptr)
            m_time += ImGui::GetIO().DeltaTime;
    }

    void BusySpinner::Begin()
    {
        // ugly check on nullptr but it's fine as long as it's not virtual (it's just to handle the smart ptr with nullptr entry)
        if (this != nullptr)
        {
            auto pos = ImGui::GetCursorScreenPos();
            m_x = pos.x;
            m_y = pos.y;
            auto dim = ImGui::GetContentRegionAvail();
            m_width = dim.x;
            m_height = dim.y;

            assert(m_isDrawing == false);
            m_isDrawing = true;
        }
    }

    void BusySpinner::End()
    {
        // ugly check on nullptr but it's fine as long as it's not virtual (it's just to handle the smart ptr with nullptr entry)
        if (this != nullptr && m_isDrawing)
        {
            ImGui::SameLine(0.0f, 0.0f);
            auto clampedRadius = core::Min(m_width * 0.5f, m_height * 0.5f, m_radius);
            float thickness = m_thickness * clampedRadius / m_radius;

            auto* drawList = ImGui::GetForegroundDrawList();

            auto scaledTime = m_time * m_speed;
            drawList->PathClear();
            auto start = int32_t(fabs(ImSin(scaledTime) * ((231 * m_numSegments) / 256)));
            const float a_min = IM_PI * 2.0f * (float(start)) / float(m_numSegments);
            const float a_max = IM_PI * 2.0f * (float((247 * m_numSegments) / 256)) / float(m_numSegments);
            const auto centre = ImVec2(m_x, m_y) + ImVec2(m_width, m_height) * 0.5f;
            for (auto i = 0; i < m_numSegments; i++)
            {
                const float a = a_min + (float(i) / float(m_numSegments)) * (a_max - a_min);
                drawList->PathLineTo({ centre.x + ImCos(a + scaledTime * 8) * clampedRadius, centre.y + ImSin(a + scaledTime * 8) * clampedRadius });
            }
            drawList->PathStroke(m_color, false, thickness);

            ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1], ImGui::GetIO().Fonts->Fonts[1]->LegacySize);

            // we write messages from the bottom of the draw region
            for (auto [messages, y] = std::make_tuple(m_messages.root, m_y + m_height - ImGui::GetFontSize() * m_messages.numMessages); messages; messages = std::atomic_ref(messages->next).load())
            {
                ImVec2 pos(m_x + ImGui::GetStyle().IndentSpacing * messages->indent, y);
                if (messages->isTextFormat)
                {
                    char buf[1024];
                    int length = messages->isParamString ? core::sprintf(buf, messages->text.c_str(), messages->str) : core::sprintf(buf, messages->text.c_str(), messages->param);
                    drawList->AddText(pos, messages->color, buf, buf + length);
                }
                else
                    drawList->AddText(pos, messages->color, messages->text.c_str(), messages->text.c_str() + messages->text.size());
                y += ImGui::GetFontSize();
            }

            ImGui::PopFont();

            m_isDrawing = false;
        }
    }

    void BusySpinner::Log(const std::string& text, uint32_t color)
    {
        auto* message = new Message{
            .text = text,
            .color = color,
            .indent = m_messages.indent,
            .isTextFormat = false,
            .next = nullptr
        };
        // one producer (current) / one consumer (root)
        if (m_messages.current == nullptr)
            std::atomic_ref(m_messages.root).store(message);
        else
            std::atomic_ref(m_messages.current->next).store(message);
        m_messages.numMessages++;
        m_messages.current = message;
    }

    BusySpinner::Message* BusySpinner::Log(const std::string& format, uint32_t color, uint32_t param)
    {
        auto* message = new Message{
            .text = format,
            .color = color,
            .indent = m_messages.indent,
            .isTextFormat = true,
            .isParamString = false,
            .param = param,
            .next = nullptr
        };
        // one producer (current) / one consumer (root)
        if (m_messages.current == nullptr)
            std::atomic_ref(m_messages.root).store(message);
        else
            std::atomic_ref(m_messages.current->next).store(message);
        m_messages.numMessages++;
        m_messages.current = message;
        return message;
    }

    BusySpinner::Message* BusySpinner::Log(const std::string& format, uint32_t color, const std::string& param)
    {
        auto strLen = uint32_t(param.size() + 1); // including nul char
        if (m_messages.stringCacheSize < strLen)
        {
            auto* stringCache = new StringCache;
            stringCache->next = m_messages.stringCache;
            m_messages.stringCache = stringCache;
            m_messages.stringCacheSize = sizeof(stringCache->buf);
        }
        char* str = m_messages.stringCache->buf + sizeof(m_messages.stringCache->buf) - m_messages.stringCacheSize;
        m_messages.stringCacheSize -= strLen;
        memcpy(str, param.c_str(), param.size());
        str[param.size()] = 0;

        auto* message = new Message{
            .text = format,
            .color = color,
            .indent = m_messages.indent,
            .isTextFormat = true,
            .isParamString = true,
            .str = str,
            .next = nullptr
        };
        // one producer (current) / one consumer (root)
        if (m_messages.current == nullptr)
            std::atomic_ref(m_messages.root).store(message);
        else
            std::atomic_ref(m_messages.current->next).store(message);
        m_messages.numMessages++;
        m_messages.current = message;
        return message;
    }

    void BusySpinner::UpdateMessageParam(Message* message, const std::string& param)
    {
        auto strLen = uint32_t(param.size() + 1); // including nul char
        if (m_messages.stringCacheSize < strLen)
        {
            auto* stringCache = new StringCache;
            stringCache->next = m_messages.stringCache;
            m_messages.stringCache = stringCache;
            m_messages.stringCacheSize = sizeof(stringCache->buf);
        }
        char* str = m_messages.stringCache->buf + sizeof(m_messages.stringCache->buf) - m_messages.stringCacheSize;
        m_messages.stringCacheSize -= strLen;
        memcpy(str, param.c_str(), param.size());
        str[param.size()] = 0;

        message->str = str;
    }
}
// namespace rePlayer