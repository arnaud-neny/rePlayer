#pragma once

#include <Containers/Array.h>
#include <string>

struct ImGuiContext;
struct ImGuiSettingsHandler;
struct ImGuiTextBuffer;
typedef int ImGuiWindowFlags;

namespace core
{
    class Window
    {
    public:
        enum class Passthrough : bool
        {
            IsNotAvailable = false,
            IsAvailable = true

        };

        struct States
        {
            bool isEnabled = false;
            bool isMinimal = false;
            bool isAlwaysOnTop = false;
            bool isPassthrough = false;
        };

        struct Handle
        {
            void* hWnd = nullptr;
            States states;

            void Apply(States other);
            void ApplyAlwaysOnTop() const;
            void ApplyPassthrough() const;
        };

        template <typename T>
        class Serialized
        {
        public:
            Serialized(const char* name, class Window* window = this);
            Serialized(const char* name, T&& defaultValue, class Window* window = this);

            T& operator=(const T& other) { m_data = other; return m_data; }
            T& operator=(T&& other) { m_data = std::move(other); return m_data; }

            operator const T& () const { return m_data; }
            operator T& () { return m_data; }

            T* operator&() { return &m_data; }

            template <typename Type>
            Type As() const { return Type(m_data); }

        private:
            T m_data;
        };

        template <typename T>
        using OnLoadedCallback = void (T::*)(void* data, const char* name);
        using OnLoadedCallbackStatic = void (*)(uintptr_t userData, void* data, const char* name);

        using OnLoadCallback = bool (*)(void* data, const char* line);
        using OnSaveCallback = void (*)(void* data, ImGuiTextBuffer* buf);

    public:
        static Passthrough ms_isPassthroughAvailable;

        Window(std::string&& name, ImGuiWindowFlags flags = 0, bool isMaster = false);
        virtual ~Window() {}

        static void Update(States states);

        void SetFlags(ImGuiWindowFlags flags, bool isEnabled);

        States GetStates() const { return m_handle.states; }

        void Enable(bool isEnabled) { m_handle.states.isEnabled = isEnabled; }

        bool IsEnabled() const { return m_handle.states.isEnabled; }
        bool IsVisible() const { return !m_handle.states.isMinimal && m_handle.states.isEnabled; }
        bool IsMinimal() const { return m_handle.states.isMinimal; }

        template <typename T>
        void RegisterSerializedData(T& data, const char* name);

        template <typename T, typename F>
        void RegisterSerializedData(T& data, const char* name, OnLoadedCallback<F> callback);

        template <typename T>
        void RegisterSerializedData(T& data, const char* name, OnLoadedCallbackStatic callback, uintptr_t userData);

        void RegisterSerializedCustomData(void* data, OnLoadCallback onLoadCallback, OnSaveCallback onSaveCallback);

    protected:
        // always called at the beginning of the frame, before all windows updates and displays
        virtual void OnBeginFrame() {}
        // always called at the beginning of the update, before displaying (or not) the window
        virtual void OnBeginUpdate() {}
        // called when the window is going to be displayed; should return the title of the window (in ImGui format)
        virtual std::string OnGetWindowTitle() = 0;
        // called when the window is displayed (inside ImGui::Begin/ImGui::End)
        virtual void OnDisplay() = 0;
        // always called at the end of the update, after displaying (or not) the window
        virtual void OnEndUpdate() {}
        // always called at the end of the frame, after all windows updates and displays
        virtual void OnEndFrame() {}
        // call when imgui has loaded the settings
        virtual void OnApplySettings() {}

    private:
        void UpdateInternal(States states);

        bool LoadSettings(const char* line);
        void SaveSettings(ImGuiTextBuffer* buf);

        static void* ImGuiIniReadOpen(ImGuiContext*, ImGuiSettingsHandler* handler, const char* name);
        static void ImGuiIniReadLine(ImGuiContext*, ImGuiSettingsHandler* handler, void* window, const char* line);
        static void ImGuiApplyAll(ImGuiContext*, ImGuiSettingsHandler* handler);
        static void ImGuiIniWriteAll(ImGuiContext*, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buf);

    private:
        Window* m_nextWindow = nullptr;
        std::string m_name;
        ImGuiWindowFlags m_flags;
        Handle m_handle;
        const bool m_isMaster;

        struct SerializedData
        {
            union
            {
                const char* name;
                OnSaveCallback onSaveCallback;
            };
            void* addr;
            uint32_t size;
            enum Type : uint16_t
            {
                kFloat,
                kInt8_t,
                kUint8_t,
                kInt16_t,
                kUint16_t,
                kInt32_t,
                kUint32_t,
                kBool,
                kString,
                kChars,
                kCustom
            } type;
            uint16_t isStaticCallback : 1 = 0;
            uintptr_t userData = 0;
            union
            {
                OnLoadedCallback<Window> onLoadedCallback = nullptr;
                OnLoadedCallbackStatic onLoadedStaticCallback;
                OnLoadCallback onLoadCallback;
            };
        };
        Array<SerializedData> m_serializedData;

        static Window* ms_windows;
    };
}
//namespace core