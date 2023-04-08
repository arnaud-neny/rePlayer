#pragma once

#include <Core.h>

class RePlayer
{
public:
    // Main
    static RePlayer* Create();
    virtual void Release() = 0;

    virtual core::Status Launch() = 0;
    virtual core::Status UpdateFrame() = 0;

    virtual void Enable(bool isEnabled) = 0;
    virtual float GetBlendingFactor() const = 0;

    // Media
    virtual void PlayPause() = 0;
    virtual void Next() = 0;
    virtual void Previous() = 0;
    virtual void Stop() = 0;
    virtual void IncreaseVolume() = 0;
    virtual void DecreaseVolume() = 0;

    // Systray
    virtual void SystrayMouseLeft(int32_t x, int32_t y) = 0;
    virtual void SystrayMouseMiddle(int32_t x, int32_t y) = 0;
    virtual void SystrayMouseRight(int32_t x, int32_t y) = 0;
    virtual void SystrayTooltipOpen(int32_t x, int32_t y) = 0;
    virtual void SystrayTooltipClose(int32_t x, int32_t y) = 0;

protected:
    RePlayer() = default;
    virtual ~RePlayer() {}
};