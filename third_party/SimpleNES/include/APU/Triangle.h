#pragma once
#include "APU/Divider.h"
#include "APU/Units.h"
#include "Cartridge.h"
#include <cstdint>

namespace sn
{

struct Triangle
{
    LengthCounter length_counter;
    LinearCounter linear_counter;

    std::uint32_t seq_idx { 0 };
    Divider       sequencer { 0 };
    int           period = 0;

    void          set_period(int p);

    // Clocked at the cpu freq
    void          clock();

    Byte          sample() const;

    int           volume() const;
    void          state(StateIO& s)
    {
        length_counter.state(s);
        linear_counter.state(s);
        s.pod(seq_idx); s.pod(period);
        sequencer.state(s);
    }
};

}
