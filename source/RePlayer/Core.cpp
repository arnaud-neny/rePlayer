// Core
#include <Core/Thread/Workers.h>

// rePlayer
#include <Database/Database.h>
#include <Playlist/Playlist.h>

#include "Core.h"

// stl
#include <atomic>

namespace rePlayer
{
    Core* Core::ms_instance = nullptr;

    void Core::Enqueue(MusicID musicId)
    {
        ms_instance->m_playlist->Enqueue(musicId);
    }

    void Core::Discard(MusicID musicId)
    {
        ms_instance->m_playlist->Discard(musicId);
    }

    void Core::AddJob(const std::function<void()>& callback)
    {
        ms_instance->m_workers->AddJob(callback);
    }

    void Core::FromJob(const std::function<void()>& callback)
    {
        ms_instance->m_workers->FromJob(callback);
    }

    template <typename ItemID>
    void Core::OnNewProxy(ItemID id)
    {
        auto* item = new Stack<ItemID>::Node;
        item->id = id;
        if constexpr (std::is_same<ItemID, SongID>::value)
        {
            std::atomic_ref(item->next).exchange(std::atomic_ref(ms_instance->m_songsStack.items).exchange(item));
        }
        else
        {
            std::atomic_ref(item->next).exchange(std::atomic_ref(ms_instance->m_artistsStack.items).exchange(item));
        }
    }

    template void Core::OnNewProxy(SongID id);
    template void Core::OnNewProxy(ArtistID id);

    template <typename ItemType>
    void Core::Stack<ItemType>::Reconcile()
    {
        // as items can become proxies, we need to clean up the unused proxies
        // rather than parsing all the database, the proxies are registered in these stacks
        // and as soon as nothing else other than the database is owning the proxy
        // we kill the proxy (item is converted back to a blob) to save memory
        auto* item = std::atomic_ref(items).exchange(nullptr);
        Node* root = nullptr;
        Node* last = nullptr;
        while (item)
        {
            auto id = item->id;

            Node node;
            while (std::atomic_ref(item->busy).compare_exchange_strong(node.busy, kBusy));

            uint32_t numReconciled = 0;
            for (auto* dbPtr : ms_instance->m_db)
            {
                auto& db = *dbPtr;
                if (db.IsValid(id))
                {
                    auto* oldItem = db[id];
                    if (oldItem != nullptr)
                    {
                        if (auto newItem = oldItem->Reconcile())
                        {
                            db.Reconcile(id, newItem);
                            numReconciled++;
                        }
                    }
                }
                else
                    numReconciled++;
            }
            if (numReconciled == int32_t(DatabaseID::kCount))
                delete item;
            else
            {
                item->next = root;
                root = item;
                if (!last)
                    last = item;
            }
            item = node.next;
        }
        if (root)
            last->next = std::atomic_ref(items).exchange(root);
    }

    void Core::Reconcile()
    {
        m_songsStack.Reconcile();
        m_artistsStack.Reconcile();
    }
}
// namespace rePlayer