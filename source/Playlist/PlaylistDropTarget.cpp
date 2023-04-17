#include "PlaylistDropTarget.h"

#include <Core/Log.h>
#include <ImGui/imgui.h>
#include <ImGui/imgui_internal.h>

#include <atomic>
#include <shellapi.h>

namespace rePlayer
{
    void Playlist::DropTarget::UpdateDragDropSource(uint8_t dropId)
    {
        if (m_files.IsNotEmpty())
        {
            ImGui::SetActiveID(ImGui::GetID("PlaylistDropTarget"), ImGui::GetCurrentWindow());

            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceExtern | ImGuiDragDropFlags_SourceAllowNullID))
            {
                ImGui::SetDragDropPayload("ExternalDragAndDropPlaylist", nullptr, 0);

                if (m_files.NumItems() <= 7)
                {
                    for (uint32_t i = 0; i < m_files.NumItems(); i++)
                        ImGui::TextUnformatted(m_files[i].cbegin()._Ptr, m_files[i].cend()._Ptr);
                }
                else
                {
                    ImGui::TextUnformatted(m_files[0].cbegin()._Ptr, m_files[0].cend()._Ptr);
                    ImGui::TextUnformatted(m_files[1].cbegin()._Ptr, m_files[1].cend()._Ptr);
                    ImGui::Text("\n... %d files ...\n ", m_files.NumItems() - 4);
                    ImGui::TextUnformatted(m_files[m_files.NumItems() - 2].cbegin()._Ptr, m_files[m_files.NumItems() - 2].cend()._Ptr);
                    ImGui::TextUnformatted(m_files[m_files.NumItems() - 1].cbegin()._Ptr, m_files[m_files.NumItems() - 1].cend()._Ptr);
                }

                ImGui::EndDragDropSource();
            }

            auto mousePos = ImGui::GetMousePos();
            auto topLeft = ImGui::GetWindowPos();
            auto right = topLeft.x + ImGui::GetWindowSize().x;
            auto bottom = topLeft.y + ImGui::GetWindowSize().y;
            if (mousePos.x >= topLeft.x && mousePos.y >= topLeft.y && mousePos.x < right && mousePos.y < bottom)
                m_canDrop |= 1 << dropId;
            else
                m_canDrop &= ~(1 << dropId);
        }
    }

    Array<std::string> Playlist::DropTarget::AcquireFiles()
    {
        m_isDropped = false;
        return std::move(m_files);
    }

    ULONG Playlist::DropTarget::AddRef()
    {
        return ++std::atomic_ref(m_refCount);
    }

    ULONG Playlist::DropTarget::Release()
    {
        auto refCount = --std::atomic_ref(m_refCount);
        if (refCount == 0)
            delete this;
        return refCount;
    }

    HRESULT Playlist::DropTarget::QueryInterface(REFIID riid, void** ppvObject)
    {
        if (riid == IID_IDropTarget)
        {
            *ppvObject = this;  // or static_cast<IUnknown*> if preferred
                                // AddRef() if doing things properly
                                // but then you should probably handle IID_IUnknown as well;
            return S_OK;
        }
        *ppvObject = nullptr;
        return E_NOINTERFACE;
    };


    HRESULT Playlist::DropTarget::DragEnter(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect)
    {
        FORMATETC fmte = { CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
        STGMEDIUM stgm;

        Array<std::string> files;
        if (SUCCEEDED(pDataObj->GetData(&fmte, &stgm)))
        {
            HDROP hdrop = (HDROP)stgm.hGlobal; // or reinterpret_cast<HDROP> if preferred
            UINT file_count = DragQueryFile(hdrop, 0xFFFFFFFF, NULL, 0);

            // we can drag more than one file at the same time, so we have to loop here
            for (UINT i = 0; i < file_count; i++)
            {
                UINT numChars = DragQueryFile(hdrop, i, nullptr, 0);
                if (numChars > 0)
                {
                    std::string szFile;
                    szFile.resize(numChars);
                    DragQueryFile(hdrop, i, szFile.data(), numChars + 1);
                    files.Add(std::move(szFile));
                }
            }

            // we have to release the data when we're done with it
            ReleaseStgMedium(&stgm);

        }

        *pdwEffect &= DROPEFFECT_NONE;
        if (!files.IsEmpty())
        {
            m_files = std::move(files);

            // ImGui hooks
            auto& io = ImGui::GetIO();
            io.AddMousePosEvent((float)pt.x, (float)pt.y);
            if (grfKeyState & MK_LBUTTON)
            {
                io.MouseDown[0] = true;
                io.MouseDownDuration[0] = 1.0f;
            }
            if (grfKeyState & MK_RBUTTON)
            {
                io.MouseDown[1] = true;
                io.MouseDownDuration[1] = 1.0f;
            }
            if (grfKeyState & MK_MBUTTON)
            {
                io.MouseDown[2] = true;
                io.MouseDownDuration[2] = 1.0f;
            }
        }
        return S_OK;
    }

    HRESULT Playlist::DropTarget::DragLeave()
    {
        m_files.Clear();

        // ImGui hooks
        auto& io = ImGui::GetIO();
        io.MouseDown[0] = false;
        io.MouseDown[1] = false;
        io.MouseDown[2] = false;
        io.MouseDownDuration[0] = 0.0f;
        io.MouseDownDuration[1] = 0.0f;
        io.MouseDownDuration[2] = 0.0f;
        m_canDrop = 0;

        return S_OK;
    }

    // occurs when we drag the mouse over our applications view whilst carrying files (post Enter, pre Leave)
    HRESULT Playlist::DropTarget::DragOver(DWORD grfKeyState, POINTL pt, DWORD* pdwEffect)
    {
        // ImGui hooks
        auto& io = ImGui::GetIO();
        io.AddMousePosEvent((float)pt.x, (float)pt.y);
        if (grfKeyState & MK_LBUTTON)
        {
            io.MouseDown[0] = true;
            io.MouseDownDuration[0] = 1.0f;
        }
        if (grfKeyState & MK_RBUTTON)
        {
            io.MouseDown[1] = true;
            io.MouseDownDuration[1] = 1.0f;
        }
        if (grfKeyState & MK_MBUTTON)
        {
            io.MouseDown[2] = true;
            io.MouseDownDuration[2] = 1.0f;
        }

        *pdwEffect &= m_canDrop ? DROPEFFECT_COPY : DROPEFFECT_NONE;
        return S_OK;
    }

    // occurs when we release the mouse button to finish the drag-drop operation
    HRESULT Playlist::DropTarget::Drop(IDataObject* /*pDataObj*/, DWORD grfKeyState, POINTL /*pt*/, DWORD* pdwEffect)
    {
        // ImGui hooks
        auto& io = ImGui::GetIO();
        io.MouseDown[0] = false;
        io.MouseDown[1] = false;
        io.MouseDown[2] = false;
        io.MouseDownDuration[0] = 0.0f;
        io.MouseDownDuration[1] = 0.0f;
        io.MouseDownDuration[2] = 0.0f;

        m_isDropped = m_canDrop;
        m_isAcceptingAll = grfKeyState & MK_CONTROL;
        m_isOverriding = grfKeyState & MK_SHIFT;

        *pdwEffect &= m_canDrop ? DROPEFFECT_COPY : DROPEFFECT_NONE;
        m_canDrop = 0;
        return S_OK;
    }

    void Playlist::DropTarget::OnCreateWindow(ImGuiViewport* vp)
    {
        RegisterDragDrop(HWND(vp->PlatformHandle), this);
    }

    void Playlist::DropTarget::OnDestroyWindow(ImGuiViewport* vp)
    {
        RevokeDragDrop(HWND(vp->PlatformHandle));
    }
}
// namespace rePlayer