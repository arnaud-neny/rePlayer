#include "Log.h"

#include <Core/String.h>
#include <Core/Window.inl.h>
#include <IO/File.h>
#include <ImGui.h>

#include <chrono>
#include <ctime>
#include <filesystem>

#include <windows.h>

namespace core
{
    char Log::ms_callbackBuf[STB_SPRINTF_MIN];
    Log* Log::ms_instance{ nullptr };

    static const char* s_logType[] = {
            "[v]",
            "[i]",
            "[w]",
            "[e]"
    };

    void Log::DisplaySettings()
    {
        bool isEnabled = ms_instance->m_mode == Mode::kFull;
        ImGui::Checkbox("Display Log", &isEnabled);
        if (isEnabled)
            ms_instance->m_mode = Mode::kFull;
        else
            ms_instance->m_mode = Mode::kFileOnly;
        uint32_t logMin = 1;
        uint32_t logMax = 100;
        ImGui::SameLine();
        ImGui::SliderScalar("##lifetime", ImGuiDataType_U32, &ms_instance->m_filesLifetime, &logMin, &logMax, "%u days retention", ImGuiSliderFlags_None);
    }

    Log::Log()
        : Window("Log", ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground)
    {
        static_assert(NumItemsOf(s_logType) == LogType::kCount);

        time_t sessionTime;
        std::time(&sessionTime);
        m_timeRef = sessionTime;
        tm localTime;
        localtime_s(&localTime, &sessionTime);
        char buf[64];
        auto len = stbsp_sprintf(buf, "logs/%04d%02d%02d%02d%02d%02d.txt", localTime.tm_year + 1900, localTime.tm_mon + 1, localTime.tm_mday, localTime.tm_hour, localTime.tm_min, localTime.tm_sec);
        len++;
        m_filename = new wchar_t[len];
        ::MultiByteToWideChar(CP_UTF8, 0, buf, len, m_filename, len);
    }

    Log::~Log()
    {
        delete[] m_filename;
    }

    std::string Log::OnGetWindowTitle()
    {
        ImGui::SetNextWindowPos(ImGui::GetMousePos(), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(320.0f, 100.0f), ImGuiCond_FirstUseEver);

        return "Log";
    }

    void Log::OnDisplay()
    {
        if (m_mode == Mode::kFileOnly || m_mode == Mode::kDisabled)
            return;

        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);

        ImGui::BeginChild("scrolling", ImVec2(0, 0), ImGuiChildFlags_None);

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

        uint32_t numLines = m_isLineInProgress ? m_currentLine.size + 1 : m_currentLine.size;
        uint32_t color = 0xffFFff;
        ImGui::PushStyleColor(ImGuiCol_Text, 0xffFFffFF);

        ImGuiListClipper clipper;
        clipper.Begin(numLines);
        while (clipper.Step())
        {
            // we can pick the first one as iterator, all strings are linear
            PageHandle hString(m_linePages[clipper.DisplayStart].stringIndex, m_linePages[clipper.DisplayStart].stringOffset);

            for (PageHandle hLine = clipper.DisplayStart; hLine.size < uint32_t(clipper.DisplayEnd); hLine.size++)
            {
                if (color != m_stringPages[hString].id.color)
                {
                    color = m_stringPages[hString].id.color;
                    ImGui::PopStyleColor();
                    ImGui::PushStyleColor(ImGuiCol_Text, 0xff000000 | color);
                }

                auto timeStr = PrintTime(m_linePages[hLine].time);
                ImGui::TextUnformatted(timeStr.buf, timeStr.buf + timeStr.length);
                ImGui::SameLine(0.0f, 0.0f);

                PageHandle currentText(m_stringPages[hString].charIndex, m_stringPages[hString].charOffset);
                auto* buf = &m_charPages[currentText];
                ImGui::TextUnformatted(buf, buf + m_stringPages[hString].numChars);
                hString.size++;
                for (uint32_t stringIndex = 1, numStrings = m_linePages[hLine].numStrings; stringIndex < numStrings; stringIndex++, hString.size++)
                {
                    ImGui::SameLine(0.0f, 0.0f);

                    if (color != m_stringPages[hString].id.color)
                    {
                        color = m_stringPages[hString].id.color;
                        ImGui::PopStyleColor();
                        ImGui::PushStyleColor(ImGuiCol_Text, 0xff000000 | color);
                    }
                    currentText = { m_stringPages[hString].charIndex, m_stringPages[hString].charOffset };
                    buf = &m_charPages[currentText];
                    ImGui::TextUnformatted(buf, buf + m_stringPages[hString].numChars);
                }
            }
        }
        clipper.End();

        ImGui::PopStyleColor();
        ImGui::PopStyleVar();

        if (m_lastLineDisplayed != numLines)
        {
            float item_pos_y = clipper.StartPosY + clipper.ItemsHeight * (numLines - 1);
            ImGui::SetScrollFromPosY(item_pos_y - ImGui::GetWindowPos().y);
            m_lastLineDisplayed = numLines;
        }

        ImGui::EndChild();
        ImGui::PopFont();
    }

    void Log::OnApplySettings()
    {
        // with ImGui, the configuration load happens once at the beginning, so clear the older logs
        auto now = std::chrono::sys_days(std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now()));
        auto mainPath = std::filesystem::current_path();
        if (std::filesystem::exists(mainPath / "logs/"))
        {
            for (const std::filesystem::directory_entry& dirEntry : std::filesystem::directory_iterator(mainPath / "logs/"))
            {
                if (dirEntry.is_regular_file())
                {
                    int32_t year, month, day;
                    if (sscanf_s(reinterpret_cast<const char*>(dirEntry.path().filename().u8string().c_str()), "%04d%02d%02d", &year, &month, &day) == 3)
                    {
                        auto fileTime = std::chrono::year{ year } / month / day;
                        auto days = now - std::chrono::sys_days{ fileTime };
                        if (days.count() > int32_t(m_filesLifetime))
                            std::filesystem::remove(dirEntry);
                    }
                }
            }
        }

        // and flush the current log depending on our new settings
        if (m_mode == Mode::kUndefined || m_mode < Mode::kFileOnly)
            m_mode = Mode::kFull;
        if (m_mode >= Mode::kFileOnly)
        {
            auto file = io::File::OpenForAppend(m_filename);
            file.Write(m_undefined.Items(), m_undefined.NumItems());
        }
        m_undefined.Reset();
    }

    void Log::Printf(LogType type, uint32_t color, const char* const format, ...)
    {
        if (m_mode == Mode::kDisabled)
            return;

        // get text buffer
        char* buf;
        uint32_t charMaxSize = m_charPages.Size();
        uint32_t charSize = m_currentChar.size;
        if (charMaxSize == charSize)
        {
            m_charPages.Push();
            buf = &m_charPages[m_currentChar];
        }
        else if (charMaxSize - charSize >= STB_SPRINTF_MIN)
            buf = &m_charPages[m_currentChar];
        else
            buf = ms_callbackBuf; // current buffer is not big enough for stb, fall back on temporary

        // check string and line break
        if (m_currentStringId.type != type)
        {
            // different type break the current line (and the current string)
            m_currentStringId.type = type;
            m_currentStringId.color = color;
            if (m_isLineInProgress)
                CloseLine();
        }
        else if (m_currentStringId.color != color)
        {
            // different color break the current string
            m_currentStringId.color = color;
            if (m_isLineInProgress)
            {
                m_linePages[m_currentLine].numStrings++;
                if (++m_currentString.size == m_stringPages.Size())
                    m_stringPages.Push();

                m_stringPages[m_currentString].id = m_currentStringId;
                m_stringPages[m_currentString].charIndex = m_currentChar.index;
                m_stringPages[m_currentString].charOffset = m_currentChar.offset;
            }
        }

        // printf
        va_list va;
        va_start(va, format);
        stbsp_vsprintfcb(StbCallback, this, buf, format, va);
        va_end(va);

        // temporary close the current line
        if (m_mode != Mode::kDisabled && m_mode != Mode::kFileOnly)
            m_stringPages[m_currentString].numChars = m_currentChar.size - PageHandle(m_stringPages[m_currentString].charIndex, m_stringPages[m_currentString].charOffset).size;
    }

    Log::TimeStr Log::PrintTime(uint32_t relativeTime)
    {
        time_t t = m_timeRef + relativeTime;
        tm localTime;
        localtime_s(&localTime, &t);
        TimeStr timeStr;
        timeStr.length = uint32_t(std::strftime(timeStr.buf, sizeof timeStr.buf, "[%D %T] ", &localTime));
        return timeStr;
    }

    void Log::NewLine()
    {
        if (m_currentLine.size == m_linePages.Size())
            m_linePages.Push();
        if (m_currentString.size == m_stringPages.Size())
            m_stringPages.Push();

        m_stringPages[m_currentString].id = m_currentStringId;
        m_stringPages[m_currentString].charIndex = m_currentChar.index;
        m_stringPages[m_currentString].charOffset = m_currentChar.offset;

        time_t currentTime;
        std::time(&currentTime);
        m_linePages[m_currentLine].time = uint32_t(currentTime - m_timeRef);
        m_linePages[m_currentLine].numStrings = 1;
        m_linePages[m_currentLine].stringIndex = m_currentString.index;
        m_linePages[m_currentLine].stringOffset = m_currentString.offset;

        m_isLineInProgress = true;
    }

    void Log::CloseLine()
    {
        m_stringPages[m_currentString].numChars = m_currentChar.size - PageHandle(m_stringPages[m_currentString].charIndex, m_stringPages[m_currentString].charOffset).size;

        if (m_stringPages[m_currentString].numChars > 0)
        {
            if (m_mode >= Mode::kFileOnly)
            {
                auto file = io::File::OpenForAppend(m_filename);
                file.Write(s_logType[m_stringPages[m_currentString].id.type], 3);
                auto timeStr = PrintTime(m_linePages[m_currentLine].time);
                file.Write(timeStr.buf, timeStr.length);
                PageHandle h(m_stringPages[m_currentString].charIndex, m_stringPages[m_currentString].charOffset);
                file.Write(&m_charPages[h], m_stringPages[m_currentString].numChars);
                char returnChar = '\n';
                file.Write(&returnChar, 1);
            }
            else if (m_mode == Mode::kUndefined)
            {
                auto timeStr = PrintTime(m_linePages[m_currentLine].time);
                m_undefined.Add(s_logType[m_stringPages[m_currentString].id.type], 3);
                m_undefined.Add(timeStr.buf, timeStr.length);
                PageHandle h(m_stringPages[m_currentString].charIndex, m_stringPages[m_currentString].charOffset);
                m_undefined.Add(&m_charPages[h], m_stringPages[m_currentString].numChars);
                m_undefined.Add('\n');
            }
        }

        if (m_mode == Mode::kDisabled || m_mode == Mode::kFileOnly)
        {
            m_charPages.v.Clear();
            m_stringPages.v.Clear();
            m_linePages.v.Clear();

            m_currentChar = {};
            m_currentString = {};
            m_currentLine = {};

            m_currentStringId = { 0 };

            m_lastLineDisplayed = 0;
        }
        else
        {
            m_currentLine.size++;
            m_currentString.size++;
        }

        m_isLineInProgress = false;
    }

    void Log::NewChars()
    {
        m_stringPages[m_currentString].numChars = m_currentChar.size - PageHandle(m_stringPages[m_currentString].charIndex, m_stringPages[m_currentString].charOffset).size;
        m_currentString.size++;
        if (m_currentString.offset == 0)
            m_stringPages.Push();
        m_linePages[m_currentLine].numStrings++;

        m_stringPages[m_currentString].id = m_currentStringId;
        m_stringPages[m_currentString].charIndex = m_currentChar.index;
        m_stringPages[m_currentString].charOffset = m_currentChar.offset;

        m_charPages.Push();
    }

    char* Log::ProcessString(const char* buf, int32_t len)
    {
        uint32_t size = m_charPages.Size() - m_currentChar.size; // 0 to 65536
        char* dst = &m_charPages[m_currentChar];

        if (buf == ms_callbackBuf)
        {
            while (len)
            {
                if (!m_isLineInProgress)
                {
                    NewLine();
                }
                auto c = *buf++;
                if (c == '\n')
                {
                    CloseLine();
                }
                else
                {
                    if (!size)
                    {
                        NewChars();
                        size = 65536;
                        dst = &m_charPages[m_currentChar];
                    }
                    *dst++ = c;
                    m_currentChar.size++;
                    size--;
                }
                len--;
            }
        }
        else
        {
            while (len)
            {
                if (!m_isLineInProgress)
                {
                    NewLine();
                }
                if (*dst++ == '\n')
                {
                    CloseLine();
                    if (m_mode != Mode::kFileOnly)
                        m_currentChar.size++;
                }
                else
                    m_currentChar.size++;
                size--;
                len--;
            }
        }
        if (size < STB_SPRINTF_MIN)
            dst = ms_callbackBuf;
        return dst;
    }
}
// namespace core