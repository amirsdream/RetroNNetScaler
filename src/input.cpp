#include "input.h"

#include <windows.h>

#include "Controller.h"

namespace scaler
{
std::uint8_t pollController1()
{
    using sn::Controller;
    std::uint8_t mask = 0;

    auto pressed = [](int vk) -> bool { return (GetAsyncKeyState(vk) & 0x8000) != 0; };

    if (pressed('J'))
        mask |= 1 << Controller::A;
    if (pressed('K'))
        mask |= 1 << Controller::B;
    if (pressed(VK_RSHIFT))
        mask |= 1 << Controller::Select;
    if (pressed(VK_RETURN))
        mask |= 1 << Controller::Start;
    if (pressed('W'))
        mask |= 1 << Controller::Up;
    if (pressed('S'))
        mask |= 1 << Controller::Down;
    if (pressed('A'))
        mask |= 1 << Controller::Left;
    if (pressed('D'))
        mask |= 1 << Controller::Right;

    return mask;
}
}
