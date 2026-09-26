/*
 * IR Remote Control Key Codes
 * NEC protocol codes for GBSC-Pro remote
 *
 * Original: Karabanov Aleksandr (2023-09-10)
 */

#ifndef IR_REMOTE_H_
#define IR_REMOTE_H_

#include <stdint.h>

// Navigation keys
#define IR_KEY_UP      0xEA52C03F
#define IR_KEY_DOWN    0xEA5220DF
#define IR_KEY_LEFT    0xEA52807F
#define IR_KEY_RIGHT   0xEA5240BF
#define IR_KEY_OK      0xEA52A05F

// Menu keys
#define IR_KEY_MENU    0xEA52609F
#define IR_KEY_EXIT    0xEA52E01F
#define IR_KEY_INFO    0xEA5210EF
#define IR_KEY_SAVE    0xEA5200FF

// Audio keys
#define IR_KEY_MUTE    0xEA52D02F
#define IR_KEY_VOL_UP  0xEA52906F
#define IR_KEY_VOL_DN  0xEA5250AF

// ====================================================================================
// No-op IR receiver (old GBSC has NO IR hardware).
//
// Provides the minimal IRrecv / decode_results API the Pro code expects so the
// IRremoteESP8266 library can be dropped while every symbol still resolves.
// decode() always returns false => a key is never "decoded" => IR path is inert.
// Only the members actually used by the code are provided (value, repeat).
// ====================================================================================

struct decode_results {
    uint32_t value;   // decoded NEC key (0 = none); 0xFFFFFFFF = NEC repeat marker
    bool     repeat;  // NEC repeat flag
    decode_results() : value(0), repeat(false) {}
};

class IRrecv {
public:
    explicit IRrecv(uint8_t pin) { (void)pin; }
    void enableIRIn() {}
    bool decode(decode_results* results) { (void)results; return false; }
    void resume() {}
};

#endif // IR_REMOTE_H_
