#pragma once

#include <Core.h>
#include <Containers/Array.h>
#include <Core/SharedContext.h>
#include <Core/Window.h>
#include <string>

namespace core
{
    class Log : public Window, public SharedContext
    {
        friend class SharedContexts;
    private:
        enum LogType : uint32_t
        {
            kVerbose = 0,
            kInfo,
            kWarning,
            kError,
            kCount
        };

    public:
        static bool IsEnabled() { return ms_instance != nullptr && ms_instance->Window::IsEnabled(); }
        static Log& Get() { return *ms_instance; }

        template <typename... Args>
        static void Verbose(uint32_t color, const char* const format, Args&&... args) { ms_instance->Printf(kVerbose, color, format, std::forward<Args>(args)...); }
        template <typename... Args>
        static void Message(const char* const format, Args&&... args) { ms_instance->Printf(kVerbose, 0x7f7f7f, format, std::forward<Args>(args)...); }
        template <typename... Args>
        static void Info(const char* const format, Args&&... args) { ms_instance->Printf(kInfo, 0xffFFff, format, std::forward<Args>(args)...); }
        template <typename... Args>
        static void Warning(const char* const format, Args&&... args) { ms_instance->Printf(kWarning, 0x00ffff, format, std::forward<Args>(args)...); }
        template <typename... Args>
        static void Error(const char* const format, Args&&... args) { ms_instance->Printf(kError, 0x0000ff, format, std::forward<Args>(args)...); }

        static void DisplaySettings();

    private:
        struct TimeStr
        {
            char buf[28];
            uint32_t length;
        };

        enum class Mode : uint8_t
        {
            kUndefined,
            kDisabled,
            kMemoryOnly,
            kFileOnly,
            kFull
        };

    private:
        Log();
        ~Log();

        std::string OnGetWindowTitle() override;
        void OnDisplay() override;
        void OnApplySettings() override;

        void Printf(LogType type, uint32_t color, const char* const format, ...);

        TimeStr PrintTime(uint32_t relativeTime);

        void NewLine();
        void CloseLine();

        void NewChars();

        static char* StbCallback(const char* buf, void* user, int32_t len) { return reinterpret_cast<Log*>(user)->ProcessString(buf, len); }
        char* ProcessString(const char* buf, int32_t len);

    private:
        union StringID
        {
            uint32_t value = 0xffFFffFF;
            struct
            {
                uint32_t color : 24;
                LogType type : 8;
            };
        };
        struct String
        {
            StringID id;
            uint16_t charIndex;
            uint16_t charOffset;
            uint32_t numChars;
        };
        struct Line
        {
            uint32_t time;
            uint32_t numStrings : 8;
            uint32_t stringIndex : 8;
            uint32_t stringOffset : 16;
        };

        template <typename T>
        struct Page
        {
            T* data = new T[65536];

            ~Page() { delete[] data; }

            Page() = default;
            Page(const Page&) = delete;
            Page(Page&& other) : data(other.data) { other.data = nullptr; }

            Page& operator=(const Page&) = delete;
            Page& operator=(Page&& other) { delete[] data; data = other.data; other.data = nullptr; }
        };

        union PageHandle
        {
            uint32_t size = 0;
            struct
            {
                uint16_t offset;
                uint16_t index;
            };

            PageHandle() = default;
            PageHandle(uint32_t otherSize) : size(otherSize) {}
            PageHandle(uint16_t otherIndex, uint16_t otherOffset) : index(otherIndex), offset(otherOffset) {}
        };

        template <typename T>
        struct Pages
        {
            T& operator[](PageHandle h) { return v[h.index].data[h.offset]; }
            const T& operator[](PageHandle h) const { return v[h.index].data[h.offset]; }

            uint32_t Size() const { return v.NumItems() * 65536; }
            void Push() { v.Push(); }

            Array<Page<T>> v;
        };

        Pages<char> m_charPages;
        Pages<String> m_stringPages;
        Pages<Line> m_linePages;

        Array<char> m_undefined;

        PageHandle m_currentChar;
        PageHandle m_currentString;
        PageHandle m_currentLine;
        bool m_isLineInProgress = false;

        Serialized<Mode> m_mode = { "Mode", Mode::kUndefined };
        Serialized<uint32_t> m_filesLifetime = { "Lifetime", 15 };

        StringID m_currentStringId = { 0 };

        uint32_t m_lastLineDisplayed = 0;

        uint64_t m_timeRef;
        wchar_t* m_filename;

        static char ms_callbackBuf[];
        static Log* ms_instance;
    };
}
// namespace core