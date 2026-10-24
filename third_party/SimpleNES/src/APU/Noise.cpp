#include "APU/Noise.h"
#include "APU/Divider.h"
#include "Cartridge.h"

namespace sn
{

void Noise::set_period_from_table(int idx)
{
    const static int periods[] {
        4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068,
    };

    divider.set_period(periods[idx] - 1);
}

void Noise::clock()
{
    if (!divider.clock())
    {
        return;
    }

    const int other    = mode ? (shift_register >> 6) & 1 : (shift_register >> 1) & 1;
    const int feedback = (shift_register & 1) ^ other;
    shift_register     = (shift_register >> 1) | (feedback << 14);
}

Byte Noise::sample() const
{
    if (length_counter.muted() || (shift_register & 1))
    {
        return 0;
    }
    return volume.get();
}

}
