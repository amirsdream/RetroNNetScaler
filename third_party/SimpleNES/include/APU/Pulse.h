#pragma once

#include "APU/Divider.h"
#include "APU/PulseUnits.h"
#include "APU/Units.h"
#include "Cartridge.h"
#include <cstddef>

namespace sn
{

using std::size_t;

struct Pulse
{
    Volume          volume;
    LengthCounter   length_counter;

    size_t          seq_idx { 0 };
    PulseDuty::Type seq_type { PulseDuty::Type::SEQ_50 };
    Divider         sequencer { 0 };
    int             period = 0;

    enum class Type
    {
        Pulse1 = 1,
        Pulse2 = 2,
    } type;

    Pulse(Type type)
      : type(type)
      , sweep(*this, type == Type::Pulse1)
    {
    }

    Sweep sweep;

    void  set_period(int p);

    // Clocked at half the cpu freq
    void  clock();

    Byte  sample() const;
    void  state(StateIO& s)
    {
        volume.state(s);
        length_counter.state(s);
        s.pod(seq_idx); s.pod(seq_type); s.pod(period);
        sequencer.state(s);
        sweep.state(s);
    }
};

}
