#ifndef SN_COLOR_H
#define SN_COLOR_H

#include <cstdint>

namespace sn
{
// RGBA8 color matching SimpleNES palette packing (0xRRGGBBAA).
struct Color
{
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 255;

    Color() = default;

    explicit Color(std::uint32_t rgba)
      : r(static_cast<std::uint8_t>((rgba >> 24) & 0xff))
      , g(static_cast<std::uint8_t>((rgba >> 16) & 0xff))
      , b(static_cast<std::uint8_t>((rgba >> 8) & 0xff))
      , a(static_cast<std::uint8_t>(rgba & 0xff))
    {
    }

    Color(std::uint8_t red, std::uint8_t green, std::uint8_t blue, std::uint8_t alpha = 255)
      : r(red)
      , g(green)
      , b(blue)
      , a(alpha)
    {
    }

    static Color Magenta() { return Color(255, 0, 255, 255); }
    static Color White() { return Color(255, 255, 255, 255); }

    std::uint32_t toRGBA8() const
    {
        return (std::uint32_t(r) << 24) | (std::uint32_t(g) << 16) | (std::uint32_t(b) << 8) | std::uint32_t(a);
    }
};
}

#endif
