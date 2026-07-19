#include "VirtualScreen.h"

namespace sn
{
void VirtualScreen::create(unsigned int w, unsigned int h, float, Color)
{
    m_width  = w;
    m_height = h;
    m_pixels.assign(static_cast<std::size_t>(w) * h * 4, 0);
}

void VirtualScreen::setPixel(std::size_t x, std::size_t y, Color color)
{
    if (x >= m_width || y >= m_height)
        return;

    const std::size_t index           = (y * m_width + x) * 4;
    m_pixels[index + 0]               = color.r;
    m_pixels[index + 1]               = color.g;
    m_pixels[index + 2]               = color.b;
    m_pixels[index + 3]               = color.a;
}
}
