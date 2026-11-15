#ifndef SAMPLE_H
#define SAMPLE_H

struct SampleI16
{
    short left, right;
};
struct SampleU16
{
    unsigned short left, right;
};
struct SampleI32
{
    int left, right;

    int& operator[](int index) { return (&left)[index]; }
    int operator[](int index) const { return (&left)[index]; }

    SampleI32 operator+(SampleI32 other) const { return { .left = left + other.left, .right = right + other.right }; }
    SampleI32 operator-(SampleI32 other) const { return { .left = left - other.left, .right = right - other.right }; }
    SampleI32 operator*(SampleI32 other) const { return { .left = left * other.left, .right = right * other.right }; }
    SampleI32 operator+(int value) const { return { .left = left + value, .right = right + value }; }
    SampleI32 operator*(int value) const { return { .left = left * value, .right = right * value }; }
    SampleI32 operator/(int value) const { return { .left = left / value, .right = right / value }; }

    friend SampleI32 operator*(int l, SampleI32 r) { return { .left = r.left * l, .right = r.right * l}; }

    SampleI32 operator>>(int value) const { return { .left = left >> value, .right = right >> value }; }
    SampleI32 operator+=(SampleI32 other) { left += other.left; right += other.right; return *this; }
    SampleI32 operator-=(SampleI32 other) { left -= other.left; right -= other.right; return *this; }
};

#endif // SAMPLE_H
