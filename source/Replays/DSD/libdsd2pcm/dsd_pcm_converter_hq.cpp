/*
    Copyright 2015-2016 Robert Tari <robert@tari.in>
    Copyright 2012 Vladislav Goncharov <vl-g@yandex.ru> (HQ DSD->PCM converter 88.2/96 kHz)

    This file is part of SACD.

    SACD is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    SACD is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with SACD.  If not, see <http://www.gnu.org/licenses/gpl-3.0.txt>.
*/

#include <string.h>
#include <assert.h>
#include "dsd_pcm_converter_hq.h"

dsdpcm_converter_hq::dsdpcm_converter_hq(): m_dither24(24)
{
    m_decimation = 0;
    m_upsampling = 0;
    m_nChannels = 0;
    m_nDsdSamplerate = 0;
    m_nPcmSamplerate = 0;
    conv_called = false;

    memset(m_resampler, 0, sizeof(m_resampler));

    // fill bits_table
    for (int i = 0; i < 16; i++)
    {
        for (int j = 0; j < 4; j++)
            m_bits_table[i][j] = (i & (1 << (3 - j))) ? 1.0 : -1.0;
    }

    for (int i = 0; i < 256; i++)
    {
        swap_bits[i] = 0;

        for (int j = 0; j < 8; j++)
        {
            swap_bits[i] |= ((i >> j) & 1) << (7 - j);
        }
    }
}

dsdpcm_converter_hq::~dsdpcm_converter_hq()
{
    int i;

    for (i = 0; i < DSDPCM_MAX_CHANNELS; i++)
    {
        delete m_resampler[i];
    }
}

float dsdpcm_converter_hq::get_delay()
{
    return (m_resampler[0] != NULL) ? (float)(m_resampler[0]->getFirSize() / 2) / (float)m_decimation : 0;
}

bool dsdpcm_converter_hq::is_convert_called()
{
    return conv_called;
}

int dsdpcm_converter_hq::init(int channels, int dsd_samplerate, int pcm_samplerate)
{
    double *impulse;
    int i, taps, sinc_freq, multiplier, divisor;

    this->m_nChannels = channels;
    this->m_nDsdSamplerate = dsd_samplerate;
    this->m_nPcmSamplerate = pcm_samplerate;

    switch (dsd_samplerate)
    {
        case DSDxFs64:
        {
            switch (pcm_samplerate)
            {
                case 96000:
                    multiplier = 1;
                    divisor = 1;
                    break;
                case 192000:
                    multiplier = 1;
                    divisor = 2;
                    break;
                default:
                    return -2;
            }

            break;
        }
        case DSDxFs128:
            switch (pcm_samplerate)
            {
            case 96000:
                multiplier = 2;
                divisor = 1;
                break;
            case 192000:
                multiplier = 2;
                divisor = 2;
                break;
            default:
                return -2;
            }
            break;
        case DSDxFs256:
            switch (pcm_samplerate)
            {
            case 96000:
                multiplier = 4;
                divisor = 1;
                break;
            case 192000:
                multiplier = 4;
                divisor = 2;
                break;
            default:
                return -2;
            }
            break;
        case DSDxFs512:
            switch (pcm_samplerate)
            {
            case 96000:
                multiplier = 8;
                divisor = 1;
                break;
            case 192000:
                multiplier = 8;
                divisor = 2;
                break;
            default:
                return -2;
            }
            break;
        default:
            return -1;
            break;
    }

    // prepare filter
    for (i = 0; i < DSDPCM_MAX_CHANNELS; i++)
    {
        delete m_resampler[i];
    }

    memset(m_resampler, 0, sizeof(m_resampler));

    // default resampling mode DSD64 -> 96 (5/147 resampling)
    // actual resampling mode DSD64 * multiplier -> 96 * divisor (5 * divisor / 147 * multiplier)
    m_upsampling = 5 * divisor;
    m_decimation = 147 * multiplier;

    // generate filter
    taps = 2878 * divisor * multiplier + 1;
    sinc_freq = 70 * 5 * divisor * multiplier; // 44.1 * 64 * upsampling / x
    impulse = new double[taps];

    generateFilter(impulse, taps, sinc_freq);

    for (i = 0; i < channels; i++)
        m_resampler[i] = new ResamplerNxMx(m_upsampling, m_decimation, impulse, taps);

    delete[] impulse;

    conv_called = false;

    return 0;
}

int dsdpcm_converter_hq::convert(uint8_t* dsd_data, int dsd_samples, float* pcm_data)
{
    int pcm_samples = 0;

/* this code makes no sense
    if (!dsd_data)
    {
        for (int sample = 0; sample < dsd_samples / 2; sample++)
        {
            uint8_t temp = dsd_data[dsd_samples - 1 - sample];
            dsd_data[dsd_samples - 1 - sample] = swap_bits[dsd_data[sample]];
            dsd_data[sample] = swap_bits[temp];
        }

        return convertResample(dsd_data, dsd_samples, pcm_data);
    }
*/

    if (!conv_called)
    {
        for (int sample = 0; sample < dsd_samples; sample++)
        {
            dsd_data[sample] = swap_bits[dsd_data[dsd_samples - 1 - sample]];
        }

        convertResample(dsd_data, dsd_samples, pcm_data);

        conv_called = true;
    }

    pcm_samples = convertResample(dsd_data, dsd_samples, pcm_data);

    return pcm_samples;
}

int dsdpcm_converter_hq::convertResample(uint8_t* dsd_data, int dsd_samples, float* pcm_data)
{
    int i, pcm_samples, ch, offset, dsd_offset, pcm_offset, j;
    double dsd_input[DSDPCM_MAX_CHANNELS][MAX_RESAMPLING_IN + 8], x[DSDPCM_MAX_CHANNELS][MAX_RESAMPLING_OUT];
    uint8_t dsd8bits;
    unsigned int x_samples = 1;

    pcm_samples = (dsd_samples * 8 + m_decimation - 1) / m_decimation / m_nChannels * m_upsampling;
    dsd_offset = 0;
    pcm_offset = 0;
    offset = 0; // offset in dsd_input

    // all PCM samples
    for (i = 0; i < pcm_samples; i += x_samples)
    {
        // fill decimation buffer for downsampling
        for (; offset < m_decimation; offset += 8)
        {
            // has padding of 8 elements
            // all channels
            for (ch = 0; ch < m_nChannels; ch++)
            {
                if (dsd_offset < dsd_samples)
                    dsd8bits = dsd_data[dsd_offset++];
                else
                    dsd8bits = 0;

                // fastfill doubles from bits
                memcpy(&dsd_input[ch][offset], m_bits_table[(dsd8bits & 0xf0) >> 4], 4 * sizeof(double));
                memcpy(&dsd_input[ch][offset + 4], m_bits_table[dsd8bits & 0x0f], 4 * sizeof(double));
            }
        }

        // now fill pcm samples in all channels!!!
        for (ch = 0; ch < m_nChannels; ch++)
        {
            m_resampler[ch]->processSample(dsd_input[ch], m_decimation, x[ch], &x_samples);

            // shift overfill in channel
            memcpy(&dsd_input[ch][0], &dsd_input[ch][m_decimation], (offset - m_decimation) * sizeof(double));
        }

        // shift offset
        offset -= m_decimation;

        // and output interleaving samples
        for (j = 0; j < (int)x_samples; j++)
        {
            for (ch = 0; ch < m_nChannels; ch++)
            {
                // interleave
                pcm_data[pcm_offset++] = (float)m_dither24.processSample(x[ch][j]);
            }
        }
    }

    assert(dsd_offset == dsd_samples);
    assert(pcm_offset == pcm_samples * m_nChannels);

    conv_called = true;

    return pcm_offset;
}
