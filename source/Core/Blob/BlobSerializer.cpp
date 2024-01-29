#include "Blob.h"
#include "BlobSerializer.h"

#include <IO/File.h>

namespace core
{
    static constexpr uint8_t zero[8] = { 0 };

    BlobSerializer::BlobSerializer()
    {
        m_patches.Push()->buffer.Resize(sizeof(Blob));
    }

    void BlobSerializer::Store(const uint8_t* items, size_t size, size_t alignment)
    {
        auto& patch = m_patches[m_currentPatch];
        patch.alignment = Max(patch.alignment, uint16_t(alignment));

        auto dataOffset = patch.buffer.NumItems();
        auto newDataOffset = (dataOffset + alignment - 1) & ~(alignment - 1);
        patch.buffer.Add(zero, newDataOffset - dataOffset);
        patch.buffer.Add(reinterpret_cast<const uint8_t*>(items), size);
    }

    void BlobSerializer::Push(size_t patchOffset, size_t numItems)
    {
        // prepare the offset
        auto currentOffset = m_patches[m_currentPatch].buffer.Size() - patchOffset;

        // next memory chunk
        auto patch = m_patches.Push();
        patch->offset = uint16_t(currentOffset);
        patch->previousPatch = m_currentPatch;
        m_currentPatch = m_patches.NumItems<uint16_t>() - 1;
        patch->numItems = uint16_t(numItems);
        patch->hasNumItems = numItems != ~0ull;
    }

    void BlobSerializer::Pop()
    {
        m_currentPatch = m_patches[m_currentPatch].previousPatch;
    }

    void BlobSerializer::Save(io::File& file)
    {
        Commit();
        auto size = m_patches[0].buffer.Size() - sizeof(Blob);
        file.WriteAs<uint16_t>(size);
        file.Write(m_patches[0].buffer.Items() + sizeof(Blob), size);
    }

    const Span<uint8_t> BlobSerializer::Buffer()
    {
        Commit();
        return { m_patches[0].buffer.Items() + sizeof(Blob), m_patches[0].buffer.Size() - sizeof(Blob) };
    }

    void BlobSerializer::Commit()
    {
        if (m_patches.NumItems() > 1)
        {
            assert(m_currentPatch == 0);
            auto& mainPatch = m_patches[0];
            uint16_t dataOffset = mainPatch.buffer.NumItems<uint16_t>();
            for (uint32_t i = 1; i < m_patches.NumItems(); i++)
            {
                auto& currentPatch = m_patches[i];

                auto newDataOffset = (dataOffset + currentPatch.alignment - 1) & ~(currentPatch.alignment - 1);
                if (currentPatch.hasNumItems)
                {
                    if (newDataOffset - dataOffset < sizeof(uint16_t))
                        newDataOffset = (dataOffset + currentPatch.alignment * 2 - 1) & ~(currentPatch.alignment - 1);

                    mainPatch.buffer.Copy(zero, newDataOffset - dataOffset - sizeof(uint16_t));
                    mainPatch.buffer.Copy(&currentPatch.numItems, sizeof(uint16_t));
                }
                else
                    mainPatch.buffer.Copy(zero, newDataOffset - dataOffset);
                dataOffset = uint16_t(newDataOffset);

                currentPatch.position = dataOffset;
                auto patch = reinterpret_cast<uint16_t*>(mainPatch.buffer.Items() + m_patches[currentPatch.previousPatch].position + currentPatch.offset);
                auto offset = currentPatch.position - m_patches[currentPatch.previousPatch].position - currentPatch.offset;
                assert(offset < (1 << 13)); // blob array offset is stored on 13 bits
                patch[0] |= offset;
                dataOffset += currentPatch.buffer.NumItems<uint16_t>();

                mainPatch.buffer.Add(currentPatch.buffer.Items(), currentPatch.buffer.Size());
            }
            m_patches.Resize(1);
        }
    }
}
// namespace core