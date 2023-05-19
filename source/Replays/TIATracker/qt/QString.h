#pragma once

#include <Core/String.h>

class QString : public std::string
{
public:
    template <typename... Args>
    QString(Args&&... args)
        : std::string(std::forward<Args>(args)...)
    {}

    template <typename... Args>
    QString arg(Args&&... args) const
    {
        char buf[256];
        core::sprintf(buf, c_str(), std::forward<Args>(args)...);
        return buf;
    }

    static QString number(int i)
    {
        char buf[32];
        _itoa_s(i, buf, 10);
        return buf;
    }
};