#pragma once

#include "APU/DMC.h"
#include "APU/FrameCounter.h"
#include "APU/Noise.h"
#include "APU/Pulse.h"
#include "APU/Timer.h"
#include "APU/Triangle.h"
#include "APU/spsc.hpp"
#include "AudioPlayer.h"
#include "IRQ.h"

namespace sn
{
class APU
{
public:
    Pulse        pulse1 { Pulse::Type::Pulse1 };
    Pulse        pulse2 { Pulse::Type::Pulse2 };
    Triangle     triangle;
    Noise        noise;
    DMC          dmc;

    FrameCounter frame_counter;

public:
    APU(AudioPlayer& player, IRQHandle& irq, std::function<Byte(Address)> dmcDma)
      : dmc(irq, dmcDma)
      , frame_counter(setup_frame_counter(irq))
      , audio(player)
    {
    }

    // clock at the same frequency as the cpu
    void step();

    void writeRegister(Address addr, Byte value);
    Byte readStatus();

    void state(StateIO& s)
    {
        pulse1.state(s);
        pulse2.state(s);
        triangle.state(s);
        noise.state(s);
        dmc.state(s);
        frame_counter.state(s);
        s.pod(divideByTwo);
    }

private:
    FrameCounter             setup_frame_counter(IRQHandle& irq);
    bool                     divideByTwo = false;

    AudioPlayer&             audio;
};

}
