// ScrollDetector.h: Scroll detection for screen capture optimization
//
//////////////////////////////////////////////////////////////////////

#ifndef SCROLL_DETECTOR_H
#define SCROLL_DETECTOR_H

#include <cstdint>
#include <cstring>
#include <algorithm>
#include "../common/commands.h"

// Scroll detection parameters
#define MIN_SCROLL_LINES    16      // Minimum scroll lines to detect (increased to reduce noise)
#define MAX_SCROLL_RATIO    4       // Maximum scroll = height / MAX_SCROLL_RATIO
#define MATCH_THRESHOLD     85      // Percentage of rows that must match (increased for stability)

// Horizontal scroll direction constants (for future use)
#define SCROLL_DIR_LEFT     2
#define SCROLL_DIR_RIGHT    3

// CRC32 lookup table for row hashing
static uint32_t crc32_table[256] = { 0 };
static bool crc32_table_initialized = false;

inline void InitCRC32Table()
{
    if (crc32_table_initialized) return;

    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
        }
        crc32_table[i] = crc;
    }
    crc32_table_initialized = true;
}

inline uint32_t ComputeCRC32(const uint8_t* data, size_t length)
{
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++) {
        crc = (crc >> 8) ^ crc32_table[(crc ^ data[i]) & 0xFF];
    }
    return crc ^ 0xFFFFFFFF;
}

class CScrollDetector
{
private:
    uint32_t* m_prevRowHashes;      // Hash for each row of previous frame
    uint32_t* m_currRowHashes;      // Hash for each row of current frame
    int m_width;                    // Frame width in pixels
    int m_height;                   // Frame height in pixels
    int m_bytesPerPixel;            // Bytes per pixel (typically 4 for BGRA)
    int m_stride;                   // Bytes per row
    int m_lastScrollAmount;         // Last detected scroll amount

public:
    CScrollDetector(int width, int height, int bpp = 4)
        : m_width(width), m_height(height), m_bytesPerPixel(bpp)
    {
        InitCRC32Table();
        m_stride = width * bpp;
        m_prevRowHashes = new uint32_t[height];
        m_currRowHashes = new uint32_t[height];
        m_lastScrollAmount = 0;
        memset(m_prevRowHashes, 0, height * sizeof(uint32_t));
        memset(m_currRowHashes, 0, height * sizeof(uint32_t));
    }

    ~CScrollDetector()
    {
        if (m_prevRowHashes) {
            delete[] m_prevRowHashes;
            m_prevRowHashes = nullptr;
        }
        if (m_currRowHashes) {
            delete[] m_currRowHashes;
            m_currRowHashes = nullptr;
        }
    }

    // Compute hash for each row of a frame
    void ComputeRowHashes(const uint8_t* frame, uint32_t* hashes)
    {
        for (int row = 0; row < m_height; row++) {
            hashes[row] = ComputeCRC32(frame + row * m_stride, m_stride);
        }
    }

    // Detect vertical scroll between previous and current frame
    // Returns scroll amount in pixels (positive = scroll down, negative = scroll up)
    // Returns 0 if no scroll detected
    int DetectVerticalScroll(const uint8_t* prevFrame, const uint8_t* currFrame)
    {
        // Compute hashes for current frame
        ComputeRowHashes(currFrame, m_currRowHashes);

        int maxScroll = m_height / MAX_SCROLL_RATIO;
        int bestScrollAmount = 0;
        int bestMatchCount = 0;

        // Try different scroll amounts
        for (int scrollAmount = MIN_SCROLL_LINES; scrollAmount <= maxScroll; scrollAmount++) {
            // Check scroll down (content moves up, new content at bottom)
            int matchCount = CountMatchingRows(scrollAmount);
            if (matchCount > bestMatchCount) {
                bestMatchCount = matchCount;
                bestScrollAmount = scrollAmount;
            }

            // Check scroll up (content moves down, new content at top)
            matchCount = CountMatchingRows(-scrollAmount);
            if (matchCount > bestMatchCount) {
                bestMatchCount = matchCount;
                bestScrollAmount = -scrollAmount;
            }
        }

        // Check if match quality is good enough
        int comparedRows = m_height - abs(bestScrollAmount);
        int threshold = (comparedRows * MATCH_THRESHOLD) / 100;

        if (bestMatchCount >= threshold && bestMatchCount >= MIN_SCROLL_LINES) {
            m_lastScrollAmount = bestScrollAmount;
            // Swap hash buffers for next frame
            std::swap(m_prevRowHashes, m_currRowHashes);
            return bestScrollAmount;
        }

        // No scroll detected, update previous hashes
        std::swap(m_prevRowHashes, m_currRowHashes);
        m_lastScrollAmount = 0;
        return 0;
    }

    // Update previous frame hashes (call after sending diff frame)
    void UpdatePrevHashes(const uint8_t* frame)
    {
        ComputeRowHashes(frame, m_prevRowHashes);
    }

    // Get edge region info for scroll frame
    // BMP is bottom-up: row 0 = screen bottom, row height-1 = screen top
    // scrollAmount > 0: scroll down (content moves up), new content at screen bottom = BMP row 0
    // scrollAmount < 0: scroll up (content moves down), new content at screen top = BMP high rows
    void GetEdgeRegion(int scrollAmount, int* outOffset, int* outPixelCount) const
    {
        if (scrollAmount > 0) {
            // Scroll down: new content at screen bottom = BMP row 0 (low address)
            *outOffset = 0;
            *outPixelCount = scrollAmount * m_width;
        } else {
            // Scroll up: new content at screen top = BMP high rows
            *outOffset = (m_height + scrollAmount) * m_stride;
            *outPixelCount = (-scrollAmount) * m_width;
        }
    }

    int GetLastScrollAmount() const { return m_lastScrollAmount; }
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    int GetStride() const { return m_stride; }

private:
    // Count matching rows for a given scroll amount
    // BMP is bottom-up: row 0 = screen bottom, row height-1 = screen top
    // scrollAmount > 0: scroll down (content moves up in screen)
    //   - In BMP: new content at row 0, old content shifted to higher rows
    //   - curr[scrollAmount + i] should match prev[i]
    // scrollAmount < 0: scroll up (content moves down in screen)
    //   - In BMP: new content at high rows, old content shifted to lower rows
    //   - curr[i] should match prev[i + absScroll]
    int CountMatchingRows(int scrollAmount) const
    {
        int matchCount = 0;
        int absScroll = abs(scrollAmount);

        if (scrollAmount > 0) {
            // Scroll down: curr[scrollAmount..height-1] should match prev[0..height-scrollAmount-1]
            for (int i = 0; i < m_height - absScroll; i++) {
                if (m_currRowHashes[i + absScroll] == m_prevRowHashes[i]) {
                    matchCount++;
                }
            }
        } else {
            // Scroll up: curr[0..height-scrollAmount-1] should match prev[scrollAmount..height-1]
            for (int i = 0; i < m_height - absScroll; i++) {
                if (m_currRowHashes[i] == m_prevRowHashes[i + absScroll]) {
                    matchCount++;
                }
            }
        }

        return matchCount;
    }
};

#endif // SCROLL_DETECTOR_H
