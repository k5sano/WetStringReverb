#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <cmath>

namespace DSP
{

/**
 * 分数遅延対応ディレイライン（Lagrange 3 次補間）。
 * processBlock 内でメモリアロケーション不可のため、
 * prepare() で最大サイズを確保する。
 */
class DelayLine
{
public:
    DelayLine() = default;

    void prepare (int maxDelaySamples)
    {
        bufferSize = maxDelaySamples + 4;  // 補間マージン
        buffer.resize (static_cast<size_t> (bufferSize), 0.0f);
        writePos = 0;
        clear();
    }

    void clear()
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
    }

    void setDelay (float delaySamples)
    {
        currentDelay = delaySamples;
    }

    float getDelay() const { return currentDelay; }

    void write (float sample)
    {
        buffer[static_cast<size_t> (writePos)] = sample;
        writePos = (writePos + 1) % bufferSize;
    }

    /** Lagrange 3 次補間で読み取り */
    float read() const
    {
        float readPos = static_cast<float> (writePos) - currentDelay - 1.0f;
        while (readPos < 0.0f)
            readPos += static_cast<float> (bufferSize);

        int intPart = static_cast<int> (readPos);
        float frac = readPos - static_cast<float> (intPart);

        // Lagrange 3 次補間（4 点）
        float y0 = buffer[static_cast<size_t> ((intPart - 1 + bufferSize) % bufferSize)];
        float y1 = buffer[static_cast<size_t> (intPart % bufferSize)];
        float y2 = buffer[static_cast<size_t> ((intPart + 1) % bufferSize)];
        float y3 = buffer[static_cast<size_t> ((intPart + 2) % bufferSize)];

        float d0 = frac - (-1.0f);
        float d1 = frac - 0.0f;
        float d2 = frac - 1.0f;
        float d3 = frac - 2.0f;

        float c0 = y0 * (d1 * d2 * d3) / ((-1.0f - 0.0f) * (-1.0f - 1.0f) * (-1.0f - 2.0f));
        float c1 = y1 * (d0 * d2 * d3) / ((0.0f - (-1.0f)) * (0.0f - 1.0f) * (0.0f - 2.0f));
        float c2 = y2 * (d0 * d1 * d3) / ((1.0f - (-1.0f)) * (1.0f - 0.0f) * (1.0f - 2.0f));
        float c3 = y3 * (d0 * d1 * d2) / ((2.0f - (-1.0f)) * (2.0f - 0.0f) * (2.0f - 1.0f));

        return c0 + c1 + c2 + c3;
    }

    /** 整数遅延で読み取り（高速パス） */
    float readInteger (int delaySamples) const
    {
        int readIdx = (writePos - delaySamples - 1 + bufferSize * 2) % bufferSize;
        return buffer[static_cast<size_t> (readIdx)];
    }

private:
    std::vector<float> buffer;
    int bufferSize = 0;
    int writePos = 0;
    float currentDelay = 0.0f;
};

}  // namespace DSP
