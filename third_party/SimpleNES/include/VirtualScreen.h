#ifndef VIRTUALSCREEN_H
#define VIRTUALSCREEN_H

#include "Color.h"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace sn
{
// CPU-side framebuffer written by the PPU (replaces SFML vertex screen).
class VirtualScreen
{
public:
    void create(unsigned int width, unsigned int height, float /*pixel_size*/ = 1.f, Color /*color*/ = Color::White());
    void setPixel(std::size_t x, std::size_t y, Color color);

    unsigned int        width() const { return m_width; }
    unsigned int        height() const { return m_height; }
    const std::uint8_t* rgba() const { return m_pixels.data(); }
    std::size_t         byteSize() const { return m_pixels.size(); }

private:
    unsigned int             m_width  = 0;
    unsigned int             m_height = 0;
    std::vector<std::uint8_t> m_pixels; // tightly packed RGBA8
};
}

#endif // VIRTUALSCREEN_H
