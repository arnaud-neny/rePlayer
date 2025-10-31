#pragma once

#include <Containers/SmartPtr.h>
#include <Core/RefCounted.h>
#include <string>

namespace rePlayer
{
    using namespace core;

    class BusySpinner : public RefCounted
    {
        struct Message
        {
            std::string text;
            uint32_t color;
            uint16_t indent;
            bool isTextFormat;
            bool isParamString;
            union
            {
                uint32_t param;
                char* str;
            };
            Message* next;
        };

        friend class SmartPtr<BusySpinner>;

    public:
        // common
        void Update();

        void Begin();
        void End();

        // log
        void Indent(int16_t numIndents) { m_messages.indent += numIndents; }

        void Log(const std::string& text, uint32_t color);
        [[nodiscard]] Message* Log(const std::string& format, uint32_t color, uint32_t param);
        [[nodiscard]] Message* Log(const std::string& format, uint32_t color, const std::string& param);

        void Info(const std::string& format) { Log(format, 0xff00ff00); }
        template <typename T>
        [[nodiscard]] Message* Info(const std::string& format, T&& param) { return Log(format, 0xff00ff00, std::forward<T>(param)); }
        void Error(const std::string& format) { Log(format, 0xff0000ff); }

        void UpdateMessageParam(Message* message, uint32_t param) { message->param = param; }
        void UpdateMessageParam(Message* message, const std::string& param);

    private:
        struct StringCache
        {
            char buf[65536 - sizeof(StringCache*)];
            StringCache* next;
        };

    private:
        BusySpinner(uint32_t color);
        ~BusySpinner() override;

    private:
        bool m_isDrawing = false;
        float m_radius = 48.0f;
        float m_thickness = 16.0f;
        int32_t m_numSegments = 64;
        float m_speed = 0.7f;
        float m_time = 0.0f;
        uint32_t m_color;

        float m_x;
        float m_y;
        float m_width;
        float m_height;

        struct 
        {
            Message* root = nullptr;
            Message* current = nullptr;
            uint16_t indent = 0;
            uint32_t numMessages = 0;

            uint32_t stringCacheSize = 0;
            StringCache* stringCache = nullptr;
        } m_messages;
    };
}
// namespace rePlayer