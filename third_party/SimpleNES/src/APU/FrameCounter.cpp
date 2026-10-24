#include "APU/FrameCounter.h"
#include "Log.h"

namespace sn
{

void FrameCounter::clearFrameInterrupt()
{
    if (frame_interrupt)
    {
        frame_interrupt = false;
        irq.release();
    }
};

// $4017 write. The sequencer restarts a few CPU cycles after the write (3-4 on hardware; the CPU here applies writes
// at instruction granularity, so 3 is used). In 5-step mode the quarter and half frame units are clocked at once.
void FrameCounter::reset(Mode m, bool irq_inhibit)
{
    mode              = m;
    interrupt_inhibit = irq_inhibit;
    counter           = -3;
    if (interrupt_inhibit)
    {
        clearFrameInterrupt();
    }
    if (mode == Seq5Step)
    {
        clockQuarter();
        clockHalf();
    }
}

void FrameCounter::clockQuarter()
{
    // envelopes & triangle's linear counter
    for (FrameClockable& c : frame_slots)
        c.quarter_frame_clock();
}

void FrameCounter::clockHalf()
{
    // length counters & sweep units
    for (FrameClockable& c : frame_slots)
        c.half_frame_clock();
}

void FrameCounter::setFrameInterrupt()
{
    if (interrupt_inhibit)
        return;
    frame_interrupt = true;
    irq.pull();
}

// Clocked every CPU cycle; step positions are CPU cycles after the sequencer restart (NESdev "APU Frame Counter").
void FrameCounter::clock()
{
    counter += 1;

    switch (counter)
    {
    case Q1:
        clockQuarter();
        break;
    case Q2:
        clockQuarter();
        clockHalf();
        break;
    case Q3:
        clockQuarter();
        break;
    case preQ4:
        if (mode == Seq4Step)
            setFrameInterrupt();
        break;
    case Q4:
        if (mode == Seq4Step)
        {
            clockQuarter();
            clockHalf();
            setFrameInterrupt();
        }
        break;
    case postQ4:
        if (mode == Seq4Step)
        {
            setFrameInterrupt();
            counter = 0;
        }
        break;
    case Q5:
        if (mode == Seq5Step)
        {
            clockQuarter();
            clockHalf();
        }
        break;
    case seq5step_length:
        if (mode == Seq5Step)
            counter = 0;
        break;
    }
}
}
