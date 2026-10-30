#include "love_audio.h"
#include "love_filesystem.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stb_vorbis.c"

int LoveAudio::s_sampleRate = 22050;
std::vector<LoveSource*> LoveAudio::s_sources;

LoveSource::LoveSource(const char *filename, SourceType type)
    : m_path(filename ? filename : ""), m_type(type), m_playing(false), m_looping(false),
      m_volume(1.0f), m_pitch(1.0f), m_samples(NULL), m_sampleCount(0), m_cursor(0),
      m_vorbis(NULL), m_streamData(NULL), m_streamSize(0), m_channels(2), m_sampleRate(44100),
      m_lowpass(false), m_resamplePos(0), m_resampleStep(65536), m_decodeStart(0) {
    if (m_type == SOURCE_STREAM) {
        openStream();
    } else {
        loadStatic();
    }
    LoveAudio::registerSource(this);
}

LoveSource::~LoveSource() {
    LoveAudio::unregisterSource(this);
    closeStream();
    if (m_samples) {
        free(m_samples);
        m_samples = NULL;
    }
}

LoveSource *LoveSource::clone() const {
    LoveSource *s = new LoveSource(m_path.c_str(), m_type);
    s->m_volume = m_volume;
    s->m_looping = m_looping;
    s->m_pitch = m_pitch;
    return s;
}

int16_t LoveSource::lerpSample(int16_t a, int16_t b, uint32_t frac) {
    int32_t v = (int32_t)a + (((int32_t)b - (int32_t)a) * (int32_t)frac >> 16);
    if (v > 32767) v = 32767;
    if (v < -32768) v = -32768;
    return (int16_t)v;
}

void LoveSource::loadStatic() {
    void *file_data = NULL;
    size_t file_size = 0;
    if (!LoveFilesystem::read(m_path.c_str(), &file_data, &file_size)) return;

    int channels = 0;
    int sample_rate = 0;
    short *decoded = NULL;
    int samples = stb_vorbis_decode_memory((const unsigned char*)file_data, (int)file_size,
                                           &channels, &sample_rate, &decoded);
    LoveFilesystem::freeData(file_data);

    if (samples <= 0 || !decoded) return;

    int out_rate = LoveAudio::getSampleRate();
    if (sample_rate <= 0) sample_rate = out_rate;
    size_t out_frames = (size_t)(((uint64_t)samples * (uint64_t)out_rate) / (uint64_t)sample_rate);
    if (out_frames == 0) out_frames = 1;

    m_samples = (int16_t*)malloc(out_frames * 2 * sizeof(int16_t));
    if (!m_samples) {
        free(decoded);
        return;
    }

    uint32_t step = (uint32_t)(((uint64_t)sample_rate << 16) / (uint64_t)out_rate);
    bool lowpass = (sample_rate >= out_rate * 2);
    uint32_t pos = 0;
    size_t last = (size_t)(samples - 1);
    for (size_t i = 0; i < out_frames; i++) {
        size_t idx = (size_t)(pos >> 16);
        uint32_t frac = pos & 0xFFFF;
        if (idx > last) idx = last;
        size_t idx2 = idx + 1;
        if (idx2 > last) idx2 = last;
        int16_t l, r;
        if (lowpass) {
            if (channels == 2) {
                l = (int16_t)(((int32_t)decoded[idx * 2] + decoded[idx2 * 2]) >> 1);
                r = (int16_t)(((int32_t)decoded[idx * 2 + 1] + decoded[idx2 * 2 + 1]) >> 1);
            } else {
                l = (int16_t)(((int32_t)decoded[idx] + decoded[idx2]) >> 1);
                r = l;
            }
        } else if (channels == 2) {
            l = lerpSample(decoded[idx * 2], decoded[idx2 * 2], frac);
            r = lerpSample(decoded[idx * 2 + 1], decoded[idx2 * 2 + 1], frac);
        } else {
            l = lerpSample(decoded[idx], decoded[idx2], frac);
            r = l;
        }
        m_samples[i * 2] = l;
        m_samples[i * 2 + 1] = r;
        pos += step;
    }

    m_sampleCount = out_frames;
    free(decoded);
}

void LoveSource::openStream() {
    if (m_streamData) return;
    if (!LoveFilesystem::read(m_path.c_str(), (void**)&m_streamData, &m_streamSize)) return;
    if (!m_streamData || m_streamSize == 0) return;

    int error = 0;
    m_vorbis = stb_vorbis_open_memory(m_streamData, (int)m_streamSize, &error, NULL);
    if (!m_vorbis) {
        free(m_streamData);
        m_streamData = NULL;
        m_streamSize = 0;
        return;
    }

    stb_vorbis_info info = stb_vorbis_get_info((stb_vorbis*)m_vorbis);
    m_channels = info.channels > 0 ? info.channels : 2;
    m_sampleRate = info.sample_rate > 0 ? info.sample_rate : 44100;
    int out_rate = LoveAudio::getSampleRate();
    m_lowpass = (m_sampleRate >= out_rate * 2);
    m_resampleStep = (uint32_t)(((uint64_t)m_sampleRate << 16) / (uint64_t)out_rate);
    m_decodeBuf.reserve(8192 * m_channels);
}

void LoveSource::closeStream() {
    if (m_vorbis) {
        stb_vorbis_close((stb_vorbis*)m_vorbis);
        m_vorbis = NULL;
    }
    if (m_streamData) {
        free(m_streamData);
        m_streamData = NULL;
    }
    m_streamSize = 0;
    m_decodeBuf.clear();
    m_decodeStart = 0;
}

bool LoveSource::decodeMore() {
    if (!m_vorbis) return false;
    size_t chunk = 2048;
    size_t old_samples = m_decodeBuf.size();
    m_decodeBuf.resize(old_samples + chunk * m_channels);
    int got = stb_vorbis_get_samples_short_interleaved((stb_vorbis*)m_vorbis, m_channels,
                                                       m_decodeBuf.data() + old_samples,
                                                       (int)(chunk * m_channels));
    if (got > 0) {
        m_decodeBuf.resize(old_samples + (size_t)got * m_channels);
        return true;
    }
    m_decodeBuf.resize(old_samples);
    if (m_looping) {
        stb_vorbis_seek_start((stb_vorbis*)m_vorbis);
        m_decodeBuf.resize(old_samples + chunk * m_channels);
        got = stb_vorbis_get_samples_short_interleaved((stb_vorbis*)m_vorbis, m_channels,
                                                       m_decodeBuf.data() + old_samples,
                                                       (int)(chunk * m_channels));
        if (got > 0) {
            m_decodeBuf.resize(old_samples + (size_t)got * m_channels);
            return true;
        }
        m_decodeBuf.resize(old_samples);
    }
    return false;
}

void LoveSource::play() {
    m_playing = true;
    if (m_type == SOURCE_STATIC) {
        m_cursor = 0;
    } else {
        if (!m_vorbis) openStream();
        if (m_vorbis) {
            stb_vorbis_seek_start((stb_vorbis*)m_vorbis);
            m_decodeBuf.clear();
            m_decodeStart = 0;
            m_resamplePos = 0;
        }
    }
}

void LoveSource::stop() {
    m_playing = false;
    m_cursor = 0;
}

void LoveSource::pause() {
    m_playing = false;
}

void LoveSource::mix(int16_t *buffer, size_t frames) {
    if (!m_playing) return;

    int32_t vol = (int32_t)(m_volume * 256.0f);

    if (m_type == SOURCE_STATIC) {
        if (!m_samples || m_sampleCount == 0) return;
        for (size_t i = 0; i < frames; i++) {
            if (m_cursor >= m_sampleCount) {
                if (m_looping) {
                    m_cursor = 0;
                } else {
                    m_playing = false;
                    break;
                }
            }
            int32_t left = (m_samples[m_cursor * 2 + 0] * vol) >> 8;
            int32_t right = (m_samples[m_cursor * 2 + 1] * vol) >> 8;
            int32_t out_l = buffer[i * 2 + 0] + left;
            int32_t out_r = buffer[i * 2 + 1] + right;
            if (out_l > 32767) out_l = 32767;
            else if (out_l < -32768) out_l = -32768;
            if (out_r > 32767) out_r = 32767;
            else if (out_r < -32768) out_r = -32768;
            buffer[i * 2 + 0] = (int16_t)out_l;
            buffer[i * 2 + 1] = (int16_t)out_r;
            m_cursor++;
        }
        return;
    }

    if (!m_vorbis) return;
    size_t out = 0;
    while (out < frames) {
        size_t avail = (m_decodeBuf.size() / m_channels) - m_decodeStart;
        if (avail < 2) {
            if (!decodeMore()) {
                m_playing = false;
                break;
            }
            avail = (m_decodeBuf.size() / m_channels) - m_decodeStart;
            if (avail < 2) {
                m_playing = false;
                break;
            }
        }

        size_t total_frames = m_decodeBuf.size() / m_channels;
        while (out < frames && (m_decodeStart + (m_resamplePos >> 16) + 1) < total_frames) {
            size_t base = m_decodeStart + (m_resamplePos >> 16);
            uint32_t frac = m_resamplePos & 0xFFFF;
            const int16_t *p0 = &m_decodeBuf[base * m_channels];
            const int16_t *p1 = &m_decodeBuf[(base + 1) * m_channels];
            int16_t l, r;
            if (m_lowpass) {
                if (m_channels == 1) {
                    l = (int16_t)(((int32_t)p0[0] + p1[0]) >> 1);
                    r = l;
                } else {
                    l = (int16_t)(((int32_t)p0[0] + p1[0]) >> 1);
                    r = (int16_t)(((int32_t)p0[1] + p1[1]) >> 1);
                }
            } else if (m_channels == 1) {
                l = lerpSample(p0[0], p1[0], frac);
                r = l;
            } else {
                l = lerpSample(p0[0], p1[0], frac);
                r = lerpSample(p0[1], p1[1], frac);
            }
            int32_t left = ((int32_t)l * vol) >> 8;
            int32_t right = ((int32_t)r * vol) >> 8;
            int32_t out_l = buffer[out * 2 + 0] + left;
            int32_t out_r = buffer[out * 2 + 1] + right;
            if (out_l > 32767) out_l = 32767;
            else if (out_l < -32768) out_l = -32768;
            if (out_r > 32767) out_r = 32767;
            else if (out_r < -32768) out_r = -32768;
            buffer[out * 2 + 0] = (int16_t)out_l;
            buffer[out * 2 + 1] = (int16_t)out_r;
            m_resamplePos += m_resampleStep;
            out++;
        }

        size_t consumed = m_resamplePos >> 16;
        if (consumed > 0) {
            m_decodeStart += consumed;
            m_resamplePos &= 0xFFFF;
        }
        if (m_decodeStart > 1024) {
            size_t rem = m_decodeBuf.size() - m_decodeStart * m_channels;
            if (rem > 0) {
                memmove(m_decodeBuf.data(), m_decodeBuf.data() + m_decodeStart * m_channels,
                        rem * sizeof(int16_t));
            }
            m_decodeBuf.resize(rem);
            m_decodeStart = 0;
        }
    }
}

void LoveAudio::init(int sample_rate) {
    s_sampleRate = sample_rate;
    s_sources.clear();
}

void LoveAudio::deinit() {
    s_sources.clear();
}

void LoveAudio::registerSource(LoveSource *src) {
    s_sources.push_back(src);
}

void LoveAudio::unregisterSource(LoveSource *src) {
    for (size_t i = 0; i < s_sources.size(); i++) {
        if (s_sources[i] == src) {
            s_sources.erase(s_sources.begin() + i);
            break;
        }
    }
}

void LoveAudio::play(LoveSource *src) {
    if (src) src->play();
}

void LoveAudio::stop(LoveSource *src) {
    if (src) src->stop();
    else {
        for (size_t i = 0; i < s_sources.size(); i++) {
            s_sources[i]->stop();
        }
    }
}

void LoveAudio::update(int16_t *out_buffer, size_t frames) {
    memset(out_buffer, 0, frames * 2 * sizeof(int16_t));
    for (size_t i = 0; i < s_sources.size(); i++) {
        s_sources[i]->mix(out_buffer, frames);
    }
}
