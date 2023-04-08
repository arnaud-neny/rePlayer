#include "Core.h"

#include <Database/Database.h>
#include <Playlist/Playlist.h>

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

    template <typename ItemID>
    void Core::Stack<ItemID>::Reconcile()
    {
        // as items can become proxies, we need to clean up the unused proxies
        // rather than parsing all the database, the proxies are registered in these stacks
        // and as soon as nothing else other than the database is owning the proxy
        // we kill the proxy (item is converted back to a blob) to save memory
        for (uint32_t i = 0; i < items.NumItems();)
        {
            auto id = items[i];
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
                            db.Update(id, newItem);
                            numReconciled++;
                        }
                    }
                }
                else
                    numReconciled++;
            }
            if (numReconciled == int32_t(DatabaseID::kCount))
                items.RemoveAtFast(i);
            else
                i++;
        }
    }

    void Core::Reconcile()
    {
        m_songsStack.Reconcile();
        m_artistsStack.Reconcile();
    }
}
// namespace rePlayer