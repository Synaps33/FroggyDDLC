#ifndef LOVE_AUDIO_H
#define LOVE_AUDIO_H

#include <vector>
#include <string>
#include <stdint.h>
#include <stddef.h>

enum SourceType {
    SOURCE_STATIC,
    SOURCE_STREAM
};

class LoveSource {
public:
    LoveSource(const char *filename, SourceType type);
    ~LoveSource();

    LoveSource *clone() const;

    void play();
    void stop();
    void pause();
    bool isPlaying() const { return m_playing; }

    void setVolume(float v) { m_volume = v; }
    float getVolume() const { return m_volume; }

    void setPitch(float p) { m_pitch = p; }
    float getPitch() const { return m_pitch; }

    void setLooping(bool loop) { m_looping = loop; }
    bool isLooping() const { return m_looping; }

    void mix(int16_t *buffer, size_t frames);

private:
    void loadStatic();
    void openStream();
    void closeStream();
    bool decodeMore();
    static int16_t lerpSample(int16_t a, int16_t b, uint32_t frac);

    std::string m_path;
    SourceType m_type;
    bool m_playing;
    bool m_looping;
    float m_volume;
    float m_pitch;

    int16_t *m_samples;
    size_t m_sampleCount;
    size_t m_cursor;

    void *m_vorbis;
    unsigned char *m_streamData;
    size_t m_streamSize;
    int m_channels;
    int m_sampleRate;
    bool m_lowpass;
    uint32_t m_resamplePos;
    uint32_t m_resampleStep;
    std::vector<int16_t> m_decodeBuf;
    size_t m_decodeStart;
};

class LoveAudio {
public:
    static void init(int sample_rate = 22050);
    static void deinit();

    static void update(int16_t *out_buffer, size_t frames);
    static void play(LoveSource *src);
    static void stop(LoveSource *src = NULL);

    static void registerSource(LoveSource *src);
    static void unregisterSource(LoveSource *src);

    static int getSampleRate() { return s_sampleRate; }

private:
    static int s_sampleRate;
    static std::vector<LoveSource*> s_sources;
};

#endif // LOVE_AUDIO_H
