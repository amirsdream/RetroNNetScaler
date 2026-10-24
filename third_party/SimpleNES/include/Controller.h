#ifndef CONTROLLER_H
#define CONTROLLER_H
#include <cstdint>
#include "State.h"
#include <vector>

namespace sn
{
using Byte = std::uint8_t;
class Controller
{
public:
    Controller();
    enum Buttons
    {
        A,
        B,
        Select,
        Start,
        Up,
        Down,
        Left,
        Right,
        TotalButtons,
    };

    void strobe(Byte b);
    Byte read();

    // Host feeds current button state each frame (bit i == Buttons enum).
    void setButtons(Byte buttonMask);
    void state(StateIO& s) { s.pod(m_strobe); s.pod(m_keyStates); s.pod(m_buttonMask); }

private:
    bool         m_strobe     = false;
    unsigned int m_keyStates  = 0;
    Byte         m_buttonMask = 0;
};
}

#endif // CONTROLLER_H
