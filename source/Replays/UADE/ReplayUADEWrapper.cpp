#include "ReplayUADE.h"

#include <IO/StreamMemory.h>
#include <Thread/Mutex.h>

extern "C"
{
#include <uade/uadeipc.h>
#include <uade/unixatomic.h>
}

#include <windows.h>
#include <io.h>

#include <filesystem>

#include "data.h"

extern "C" int uade_filesize(size_t * size, const char* pathname)
{
    (void)size; (void)pathname;
    assert(0 && "Not implemented");
    return -1;
}

extern "C" char* uade_dirname(char* dst, char* src, size_t maxlen)
{
    std::filesystem::path dir(src);
    dir.remove_filename();
    auto dirName = dir.string();
    memcpy(dst, dirName.c_str(), core::Min(dirName.size() + 1, maxlen));
    return dst;
}

extern "C" int uade_find_amiga_file(char* /*realname*/, size_t /*maxlen*/, const char* /*aname*/, const char* /*playerdir*/)
{
    // we use the callback, so this is never called
    assert(0);
    return -1;
}

extern "C" int uade_atomic_close(int fd)
{
    while (1) {
        if (close(fd) < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        break;
    }
    return 0;
}

struct MySocket : public core::thread::Mutex
{
    HANDLE m_semaphore;
    uint64_t m_messageOffset = 0;
    core::Array<uint8_t> m_message;
    int32_t m_count = 0;
};
static MySocket s_sockets[2];
static bool s_isClosed = false;

extern "C" ssize_t uade_atomic_read(int fd, const void* buf, size_t count)
{
    assert((fd & 0xffF0) == 0x7770);
    if (s_isClosed)
        return 0;

    auto& socket = s_sockets[fd & 1];
    auto dst = (uint8_t*)buf;
    size_t bytes_read = 0;
    while (bytes_read < count)
    {
        ::WaitForSingleObject(socket.m_semaphore, INFINITE);
        if (s_isClosed)
            return 0;
        socket.Lock();

        auto src = socket.m_message.Items() + socket.m_messageOffset;
        auto messageSize = socket.m_message.NumItems() - socket.m_messageOffset;
        auto toRead = core::Min(count - bytes_read, messageSize);
        memcpy(dst + bytes_read, src, toRead);

        bytes_read += toRead;
        socket.m_messageOffset += toRead;
        if (socket.m_messageOffset == socket.m_message.NumItems())
        {
            socket.m_message.Clear();
            socket.m_messageOffset = 0;
        }
        else
        {
            ::ReleaseSemaphore(socket.m_semaphore, 1, nullptr);
        }

        socket.Unlock();
    }
    return bytes_read;
}

extern "C" ssize_t uade_atomic_write(int fd, const void* buf, size_t count)
{
    assert((fd & 0xffF0) == 0x7770);
    if (s_isClosed)
        return 0;

    auto& socket = s_sockets[fd & 1];
    socket.Lock();

    auto offset = socket.m_message.NumItems();
    socket.m_message.Resize(offset + count);
    memcpy(socket.m_message.Items(offset), buf, count);

    socket.Unlock();
    ::ReleaseSemaphore(socket.m_semaphore, 1, nullptr);

    return count;
}

extern "C" int in_m68k_go;

extern "C" void uade_arch_kill_and_wait_uadecore(struct uade_ipc* /*ipc*/, void** userdata)
{
    while (in_m68k_go == 0)
        ::Sleep(1);

    s_isClosed = true;
    ::ReleaseSemaphore(s_sockets[0].m_semaphore, 1, nullptr);
    ::ReleaseSemaphore(s_sockets[1].m_semaphore, 1, nullptr);

    WaitForSingleObject((HANDLE)*userdata, INFINITE);
    CloseHandle((HANDLE)*userdata);

    CloseHandle(s_sockets[0].m_semaphore);
    CloseHandle(s_sockets[1].m_semaphore);
}

extern "C" int uadecore_main(int argc, char** argv);

static void thread_func()
{
    const char *args[] = { "uadecore", "-i", "30577", "-o", "30576" };
    uadecore_main(5, (char**)args);
}

extern "C" int uade_arch_spawn(struct uade_ipc *ipc, /*pid_t * uadepid*/void** userdata, const char */*uadename*/)
{
    s_sockets[0].m_semaphore = ::CreateSemaphore(nullptr, 0, 1, nullptr);
    s_sockets[1].m_semaphore = ::CreateSemaphore(nullptr, 0, 1, nullptr);

    *userdata = (void*)CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)thread_func, nullptr, 0, nullptr);

    uade_set_peer(ipc, 1, 0x7770, 0x7771);
    return 0;
}

extern "C" FILE* stdioemu_fopen(const char* filename, const char*)
{
    FILE* stream = nullptr;
    auto fixedFilename = filename;
    while (*fixedFilename == '/')
        fixedFilename++;

    for (auto& file : s_files)
    {
        if (stricmp(file.path, fixedFilename) == 0)
        {
            stream = reinterpret_cast<FILE*>(core::io::StreamMemory::Create(filename, reinterpret_cast<const uint8_t*>(file.data), file.size, true).Detach());
            break;
        }
    }

    return stream;
}

extern "C" int stdioemu_fseek(FILE* stream, long offset, int origin)
{
    return int(reinterpret_cast<core::io::Stream*>(stream)->Seek(offset, core::io::Stream::SeekWhence(origin)));
}

extern "C" int stdioemu_fread(void* buffer, size_t size, size_t count, FILE* stream)
{
    return int(reinterpret_cast<core::io::Stream*>(stream)->Read(buffer, size * count));
}

extern "C" int stdioemu_fwrite(const char*, int, int, FILE*)
{
    assert(0);
    return 0;
}

extern "C" int stdioemu_ftell(FILE* stream)
{
    return int(reinterpret_cast<core::io::Stream*>(stream)->GetPosition());
}

extern "C" int stdioemu_fclose(FILE* stream)
{
    if (stream)
        reinterpret_cast<rePlayer::io::Stream*>(stream)->Release();
    return 0;
}

extern "C" char* stdioemu_fgets(char* str, int numChars, FILE* stream)
{
    auto strm = reinterpret_cast<rePlayer::io::Stream*>(stream);
    if (strm->GetPosition() == strm->GetSize())
        return nullptr;

    auto s = str;
    numChars--;
    while (numChars > 0)
    {
        if (strm->GetPosition() == strm->GetSize())
            break;
        strm->Read(s, 1);
        s++;
        if (s[-1] == '\n')
            break;

        numChars--;
    }
    *s = 0;
    return str;
}

extern "C" int stdioemu_feof(FILE* stream)
{
    auto strm = reinterpret_cast<rePlayer::io::Stream*>(stream);
    return strm->GetPosition() >= strm->GetSize();
}

FILE* uade_fopen(const char* a, const char* b)
{
    return stdioemu_fopen(a, b);
}

int uade_fseek(FILE* a, int b, int c)
{
    return stdioemu_fseek(a, b, c);
}

int uade_fread(char* a, int b, int c, FILE* d)
{
    return stdioemu_fread(a, b, c, d);
}
int uade_fwrite(const char*, int, int, FILE*)
{
    assert(0);
    return 0;
}

int uade_ftell(FILE* a)
{
    return stdioemu_ftell(a);
}

int uade_fclose(FILE* a)
{
    return stdioemu_fclose(a);
}

char* uade_fgets(char* a, int b, FILE* c)
{
    return stdioemu_fgets(a, b, c);
}

int uade_feof(FILE* a)
{
    return stdioemu_feof(a);
}
