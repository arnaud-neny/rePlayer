#include "Stream.h"

#include <Core.h>

namespace core::io
{
    SmartPtr<Stream> Stream::Clone()
    {
        auto stream = OnClone();
        if (stream.IsValid())
            stream->m_cachedData = m_cachedData;
        return stream;
    }

    SmartPtr<Stream> Stream::Open(const std::string& filename)
    {
        if (m_root)
            return m_root->Open(filename);
        auto stream = OnOpen(filename);
        if (stream.IsValid())
            AddFilename(stream->GetName());
        return stream;
    }

    SmartPtr<Stream> Stream::Next(bool isForced)
    {
        auto stream = OnNext(isForced);
        if (stream.IsValid())
        {
            auto* root = this;
            while (root->m_root)
                root = root->m_root;
            while (root->m_files)
            {
                auto* next = root->m_files->next;
                core::Free(root->m_files);
                root->m_files = next;
            }
            AddFilename(stream->GetName());
        }
        return stream;
    }

    void Stream::Rewind()
    {
        Seek(0, kSeekBegin);

        auto* root = this;
        while (root->m_root)
            root = root->m_root;
        while (root->m_files && root->m_files->next) // keep the last one (which is the first entry)
        {
            auto* next = root->m_files->next;
            core::Free(root->m_files);
            root->m_files = next;
        }
    }

    Stream::Stream(Stream* root)
        : m_root(root)
    {}

    Stream::~Stream()
    {
        for (auto* files = m_files; files;)
        {
            auto* next = files->next;
            core::Free(files);
            files = next;
        }
    }

    Stream::SharedMemory::~SharedMemory()
    {
        Free(m_ptr);
    }

    const Span<const uint8_t> Stream::Read()
    {
        auto size = GetSize();
        if (m_cachedData.IsInvalid())
        {
            m_cachedData.New(Alloc(size_t(size)));
            Seek(0, SeekWhence::kSeekBegin);
            Read(m_cachedData->m_ptr, size);
        }
        return { reinterpret_cast<const uint8_t*>(m_cachedData->m_ptr), uint32_t(size) };
    }

    Array<std::string> Stream::GetFilenames() const
    {
        Array<std::string> files;
        auto* root = GetRoot();
        for (auto* file = root->m_files; file; file = file->next)
            files.Add(file->name);
        for (int64_t i = 0, e = files.NumItems<int64_t>() - 1; i < e; i++, e--)
            std::swap(files[i], files[e]);
        return files;
    }

    SmartPtr<Stream> Stream::OnNext(bool isForced)
    {
        (void)isForced;
        return nullptr;
    }

    void Stream::AddFilename(const std::string& filename)
    {
        auto* root = this;
        while (root->m_root)
            root = root->m_root;
        for (auto* file = m_files; file; file = file->next)
        {
            if (file->name == filename)
                return;
        }
        root->m_files = FS::Create(filename, root->m_files);
    }

    Stream::FS* Stream::FS::Create(const std::string& filename, FS* nextFS)
    {
        auto* fs = new (core::Alloc(sizeof(FS) + filename.size() + 1)) FS();
        fs->next = nextFS;
        memcpy(fs->name, filename.c_str(), filename.size() + 1);
        return fs;
    }
}
// namespace core::io