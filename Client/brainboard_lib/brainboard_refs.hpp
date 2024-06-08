#ifndef BRAINBOARD_REFS_HPP
#define BRAINBOARD_REFS_HPP

#include <stdint.h>

namespace SERIAL {

/* EYES REFS: */
    enum class EyeAnimation : uint8_t { 
        MOVE                = 0,
        BLINK_ANIM          = 1,
        CONFUSED_ANIM       = 2,
        THINKING_ANIM       = 3,
        DISABLE             = 4,
    };

    enum class EyeID : uint8_t {
        LEFT        = 0,
        RIGHT       = 1,
        BOTH        = 2,
    }; 

/* LEDSTRIP REF: */
    enum class LedstripCommandTypes {
        SET_RGB             = 0,
        ANIM_BLINK          = 1,
        ANIM_FADE_OUT       = 2,
        ANIM_FADE_IN        = 3,
    };

    enum class LedID : uint8_t {
        HEAD        = 1,
        BODY        = 2,
        // BOTH        = 2,
    }; 

/* Motor REF */
    enum class MotorID : uint8_t {
        AZIMUTH        = 0, // X axis
        ELEVATION      = 1, // Y axis
        // BOTH        = 2,
    }; 

} // namespace SERIAL
#endif // BRAINBOARD_REFS_HPP