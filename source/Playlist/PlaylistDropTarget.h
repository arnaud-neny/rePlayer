#pragma once

#include "Playlist.h"

#include <Imgui/imgui_callbacks.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <oleidl.h>
#include <string>

namespace rePlayer
{
    class Playlist::DropTarget : public IDropTarget, public ImGui::Callbacks
    {
    public:
        DropTarget(Playlist& playlist) : m_playlist(playlist) {}

        void UpdateDragDropSource(uint8_t dropId);

        bool IsEnabled() const { return m_files.IsNotEmpty(); }
        bool IsDropped() const { return m_isDropped; }
        bool IsAcceptingAll() const { return m_isAcceptingAll; }
        bool IsOverriding() const { return m_isOverriding; }
        Array<std::string> AcquireFiles();

        ULONG AddRef() override;
        ULONG Release() override;

    private:
        HRESULT QueryInterface(REFIID riid, void** ppvObject) override;

        HRESULT DragEnter(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override;
        HRESULT DragLeave() override;
        HRESULT DragOver(DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override;

        HRESULT Drop(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override;

        void OnCreateWindow(ImGuiViewport* vp) override;
        void OnDestroyWindow(ImGuiViewport* vp) override;

    private:
        Playlist& m_playlist;
        int32_t m_refCount = 0;
        bool m_isDropped = false;
        bool m_isAcceptingAll = false;
        bool m_isOverriding = false;
        uint8_t m_canDrop = 0;
        Array<std::string> m_files;
    };
}
// namespace rePlayer