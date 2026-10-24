#include "APU/DMC.h"
#include "APU/Divider.h"
#include "Cartridge.h"

namespace sn
{

void DMC::set_irq_enable(bool enable)
{
    irqEnable = enable;
    if (!irqEnable)
    {
        clear_interrupt();
    }
}

// $4015 bit 4. Writing $4015 also clears the DMC interrupt flag.
void DMC::control(bool enable)
{
    clear_interrupt();
    change_enabled = enable;
    if (!enable)
    {
        remaining_bytes = 0;
    }
    else if (remaining_bytes == 0)
    {
        current_address = sample_begin;
        remaining_bytes = sample_length;
    }
}

// Changing the rate does not restart the timer.
void DMC::set_rate(int idx)
{
    const static int rate[] { 428, 380, 340, 320, 286, 254, 226, 214, 190, 160, 142, 128, 106, 84, 72, 54 };

    change_rate.set_period(rate[idx] - 1);
}

void DMC::clear_interrupt()
{
    irq.release();
    interrupt = false;
}

Byte DMC::sample() const
{
    return volume;
}

// Clocked every CPU cycle (NESdev "APU DMC": memory reader + output unit).
void DMC::clock()
{
    fill_buffer();

    if (!change_rate.clock())
    {
        return;
    }

    if (!silenced)
    {
        if (shifter & 1)
        {
            if (volume <= 125)
                volume += 2;
        }
        else if (volume >= 2)
        {
            volume -= 2;
        }
    }
    shifter >>= 1;

    if (--remaining_bits <= 0)
    {
        // New output cycle: take the sample buffer if there is one.
        remaining_bits = 8;
        if (buffer_empty)
        {
            silenced = true;
        }
        else
        {
            silenced     = false;
            shifter      = sample_buffer;
            buffer_empty = true;
            fill_buffer();
        }
    }
}

// Memory reader: refill the sample buffer as soon as it is empty and bytes remain.
void DMC::fill_buffer()
{
    if (!buffer_empty || remaining_bytes == 0)
    {
        return;
    }

    sample_buffer   = dma(current_address);
    buffer_empty    = false;
    current_address = current_address == 0xffff ? 0x8000 : current_address + 1;

    if (--remaining_bytes == 0)
    {
        if (loop)
        {
            current_address = sample_begin;
            remaining_bytes = sample_length;
        }
        else if (irqEnable)
        {
            interrupt = true;
            irq.pull();
        }
    }
}

}
