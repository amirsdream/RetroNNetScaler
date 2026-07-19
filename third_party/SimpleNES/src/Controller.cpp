#include "Controller.h"

namespace sn
{
Controller::Controller()
  : m_strobe(false)
  , m_keyStates(0)
  , m_buttonMask(0)
{
}

void Controller::setButtons(Byte buttonMask)
{
    m_buttonMask = buttonMask;
}

void Controller::strobe(Byte b)
{
    m_strobe = (b & 1);
    if (!m_strobe)
    {
        m_keyStates = m_buttonMask;
    }
}

Byte Controller::read()
{
    Byte ret;
    if (m_strobe)
        ret = (m_buttonMask & 1);
    else
    {
        ret = (m_keyStates & 1);
        m_keyStates >>= 1;
    }
    return ret | 0x40;
}
}
