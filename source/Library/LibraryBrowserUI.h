#pragma once

#include <Library/Library.h>

struct ImGuiTextFilter;

namespace rePlayer
{
    struct BrowserContext;

    class Library::BrowserUI
    {
    public:
        BrowserUI(Library& library);
        virtual ~BrowserUI();

        void OnDisplay();

        const char* GetTitle() const;

    private:
        bool DisplayStages();
        bool DisplayFilter(Source& source, bool isDirty);
        void DisplayEntries(Source& source, bool isDirty);

    private:
        static constexpr char kHeader[] = "BrowserFilter";

    private:
        Array<ImGuiTextFilter*> m_filters;
        BrowserContext* m_context;
        std::string m_title;
        struct Stage
        {
            std::string name;
            uint32_t dbIndex;
        };
        Array<Stage> m_stages;
        int32_t m_lastSelectedEntry = -1;
        uint32_t m_dbSongsRevision = 0;
        uint32_t m_dbArtistsRevision = 0;
    };
}
// namespace rePlayer