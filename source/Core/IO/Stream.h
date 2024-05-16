#pragma once

#include <Containers/Array.h>
#include <Containers/SmartPtr.h>
#include <Containers/Span.h>
#include <Core/RefCounted.h>

#include <string>

namespace core::io
{
    class Stream : public RefCounted
    {
    public:
        enum SeekWhence
        {
            kSeekBegin,
            kSeekCurrent,
            kSeekEnd
        };

    public:
        virtual uint64_t Read(void* buffer, uint64_t size) = 0;
        virtual Status Seek(int64_t offset, SeekWhence whence) = 0;

        virtual [[nodiscard]] uint64_t GetSize() const = 0;
        virtual [[nodiscard]] uint64_t GetPosition() const = 0;

        virtual void SetName(const std::string& name) { (void)name; }
        virtual [[nodiscard]] const std::string& GetName() const = 0;

        virtual [[nodiscard]] std::string GetComments() const { return {}; }
        virtual [[nodiscard]] std::string GetInfo() const { return {}; }
        virtual [[nodiscard]] std::string GetTitle() const { return {}; }

        [[nodiscard]] SmartPtr<Stream> Clone();

        // open another stream in the same context as the current one
        [[nodiscard]] SmartPtr<Stream> Open(const std::string& filename);
        // open the next entry in the stream (eg: archive)
        [[nodiscard]] SmartPtr<Stream> Next(bool isForced = false);
        // reset all states of the stream
        void Rewind();

        // read all the stream
        virtual [[nodiscard]] const Span<const uint8_t> Read();

        [[nodiscard]] Array<std::string> GetFilenames() const;

    protected:
        Stream(Stream* root);
        virtual ~Stream();

        virtual [[nodiscard]] SmartPtr<Stream> OnOpen(const std::string& filename) = 0;
        virtual [[nodiscard]] SmartPtr<Stream> OnClone() = 0;
        virtual [[nodiscard]] SmartPtr<Stream> OnNext(bool isForced);

        void AddFilename(const std::string& filename);
        Stream* GetRoot() const { return m_root ? m_root.Get() : const_cast<Stream*>(this); }

    private:
        Stream() = delete;
        Stream(const Stream&) = delete;
        Stream& operator=(const Stream&) = delete;

    protected:
        struct SharedMemory : public RefCounted
        {
            SharedMemory(void* ptr) : m_ptr(ptr) {}
            ~SharedMemory();
            void* m_ptr;
        };
        SmartPtr<SharedMemory> m_cachedData;

    private:
        SmartPtr<Stream> m_root;

        // list of files opened in the context of a stream (root), not including the first opened file
        struct FS
        {
            static FS* Create(const std::string& filename, FS* nextFS);
            FS* next;
            char name[0];
        }* m_files = nullptr;
    };
}
// namespace core::io