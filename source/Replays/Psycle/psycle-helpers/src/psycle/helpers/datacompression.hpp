///\file
///\interface beerz77-2 algorithm.
///
/// beerz77-2 algorithm by jeremy evers, loosely based on lz77
/// -2 designates the smaller window, faster compression version
/// designed for decompression on gameboy advance
/// due to it's efficient decompression, it is usefull for many other things... like pattern data.
///
/// SoundSquash and SoundDesquash by jeremy evers
/// designed with speed in mind
/// simple, non adaptave delta predictor, less effective with high frequency content

#pragma once
#include <universalis.hpp>
#include <cstddef>
namespace psycle { namespace helpers { namespace DataCompression {

using namespace universalis::stdlib;

/// compresses.
/// returns the destination size. remember to delete the destination when done!
std::size_t BEERZ77Comp2(uint8_t const * source, uint8_t ** destination, std::size_t source_size);

/// decompresses.
/// \todo the destination size is NOT returned
/// remember to delete your destination when done!
bool BEERZ77Decomp2(uint8_t const * source, uint8_t ** destination);

/// squashes sound.
/// returns the destination size. remember to delete the destination when done!
std::size_t SoundSquash(int16_t const * source, uint8_t ** destination, std::size_t source_size);

/// desquashes sound.
/// \todo the destination size is NOT returned
/// remember to delete your destination when done!
bool SoundDesquash(uint8_t const * source, int16_t ** destination);

}}}
