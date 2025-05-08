///\interface psycle::helpers
#pragma once
#include <cassert>

namespace psycle { namespace helpers {

namespace value_mapper {
/// maps a byte (0 to 128) to a float (0 to 1).
template <typename T>
float inline map_128_1(T value) {
	return static_cast<float>(value) * 0.0078125f;
}

/// maps a byte (0 to 256) to a float (0 to 1).
template <typename T>
float inline map_256_1(T value) {
	return static_cast<float>(value) * 0.00390625f;
}
/// maps a byte (0 to 256) to a float (0 to 100).
template <typename T>
float inline map_256_100(T value) {
	return static_cast<float>(value) * 0.390625f;
}
/// maps a byte (0 to 32768) to a float (0 to 1).
template <typename T>
float inline map_32768_1(T value) {
	return static_cast<float>(static_cast<double>(value) * 0.000030517578125);
}
/// maps a byte (0 to 65536) to a float (0 to 1).
template <typename T>
float inline map_65536_1(T value) {
	return static_cast<float>(static_cast<double>(value) * 0.0000152587890625);
}

/// maps a float (0 to 1) to a byte (0 to 128).
template <typename T>
T inline map_1_128(float value) {
	return static_cast<T>(value * 128.f);
}

/// maps a float (0 to 1) to a byte (0 to 256).
template <typename T>
T inline map_1_256(float value) {
	return static_cast<T>(value * 256.f);
}
/// maps a float (0 to 1) to a int (0 to 32768).
template <typename T>
T inline map_1_32768(float value) {
	return static_cast<T>(value * 32768.f);
}
/// maps a float (0 to 1) to a int (0 to 65536).
template <typename T>
T inline map_65536_1(float value) {
	return static_cast<T>(value * 65536.f);
}

}}}
