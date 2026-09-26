// ====================================================================================
// adv_controller.h
// ADV Controller UART Communication Driver (Header-Only)
//
// Generic driver for UART communication with HC32F460 ADV controller.
// This module is project-independent and contains only:
// - Protocol constants
// - Packet definitions for ADV7280/ADV7391 control
// - ADVController class for packet transmission
//
// Protocol format: [0x41 0x44] [cmd] [data] [random] [0xFE] [checksum]
// - 0x41 0x44 ('AD'): Header bytes identifying ADV protocol
// - cmd: Command byte ('S' = Source, 'T' = TvMode, 'N' = BCSH, 'C' = Custom)
// - data: Command-specific payload
// - random: Random byte (for anti-replay / packet uniqueness)
// - 0xFE: End-of-frame marker
// - checksum: 8-bit sum of all preceding bytes
// ====================================================================================

#ifndef ADV_CONTROLLER_H_
#define ADV_CONTROLLER_H_

#include <Arduino.h>

// ====================================================================================
// Protocol Constants
// ====================================================================================

#define ADV_HEADER_0        0x41  // 'A'
#define ADV_HEADER_1        0x44  // 'D'
#define ADV_END_MARKER      0xFE
#define ADV_PACKET_SIZE     7

// Command bytes
#define ADV_CMD_SOURCE      'S'   // Input source / line mode / smooth / sync stripper
#define ADV_CMD_TVMODE      'T'   // TV mode (video format)
#define ADV_CMD_BCSH        'N'   // Brightness/Contrast/Saturation/Hue register write
#define ADV_CMD_CUSTOM      'C'   // Custom I2C batch command

// ====================================================================================
// Packet Constants - Input Sources
// Data byte format for 'S' command: 0xXY where X=source type, Y=mode bits
// ====================================================================================

static const unsigned char ADV_InputRGBs[4]  = {ADV_HEADER_0, ADV_HEADER_1, ADV_CMD_SOURCE, 0x40};
static const unsigned char ADV_InputRGsB[4]  = {ADV_HEADER_0, ADV_HEADER_1, ADV_CMD_SOURCE, 0x50};
static const unsigned char ADV_InputVGA[4]   = {ADV_HEADER_0, ADV_HEADER_1, ADV_CMD_SOURCE, 0x60};
static const unsigned char ADV_InputYpbpr[4] = {ADV_HEADER_0, ADV_HEADER_1, ADV_CMD_SOURCE, 0x70};
static const unsigned char ADV_InputSV[4]    = {ADV_HEADER_0, ADV_HEADER_1, ADV_CMD_SOURCE, 0x10};
static const unsigned char ADV_InputAV[4]    = {ADV_HEADER_0, ADV_HEADER_1, ADV_CMD_SOURCE, 0x20};

// ====================================================================================
// Packet Constants - Video Options
// ====================================================================================

static const unsigned char ADV_TvMode[4]            = {ADV_HEADER_0, ADV_HEADER_1, ADV_CMD_TVMODE, 0x00};
static const unsigned char ADV_I2P_On[4]            = {ADV_HEADER_0, ADV_HEADER_1, ADV_CMD_SOURCE, 0x30};
static const unsigned char ADV_I2P_Off[4]           = {ADV_HEADER_0, ADV_HEADER_1, ADV_CMD_SOURCE, 0x31};
static const unsigned char ADV_Smooth_On[4]         = {ADV_HEADER_0, ADV_HEADER_1, ADV_CMD_SOURCE, 0x90};
static const unsigned char ADV_Smooth_Off[4]        = {ADV_HEADER_0, ADV_HEADER_1, ADV_CMD_SOURCE, 0x91};
static const unsigned char ADV_SyncStripper_On[4]   = {ADV_HEADER_0, ADV_HEADER_1, ADV_CMD_SOURCE, 0xA0};
static const unsigned char ADV_SyncStripper_Off[4]  = {ADV_HEADER_0, ADV_HEADER_1, ADV_CMD_SOURCE, 0xA1};
static const unsigned char ADV_ACE_On[4]            = {ADV_HEADER_0, ADV_HEADER_1, ADV_CMD_SOURCE, 0x80};
static const unsigned char ADV_ACE_Off[4]           = {ADV_HEADER_0, ADV_HEADER_1, ADV_CMD_SOURCE, 0x81};
static const unsigned char ADV_BCSH[4]              = {ADV_HEADER_0, ADV_HEADER_1, ADV_CMD_BCSH, 0x00};

// ====================================================================================
// ACE (Adaptive Contrast Enhancement) Parameter Commands
// Uses 'S' command with sub-commands 0x82-0x87
// Data byte in packet[4] contains the parameter value
// ====================================================================================

#define ADV_ACE_LUMA_GAIN       0x82  // Set Luma Gain (0-31), value in packet[4]
#define ADV_ACE_CHROMA_GAIN     0x83  // Set Chroma Gain (0-15), value in packet[4]
#define ADV_ACE_CHROMA_MAX      0x84  // Set Chroma Max (0-15), value in packet[4]
#define ADV_ACE_GAMMA_GAIN      0x85  // Set Gamma Gain (0-15), value in packet[4]
#define ADV_ACE_RESPONSE_SPEED  0x86  // Set Response Speed (0-15), value in packet[4]
#define ADV_ACE_DEFAULTS        0x87  // Reset ACE parameters to defaults

// ACE parameter default values (from ADV7280 User Sub Map 2)
#define ADV_ACE_LUMA_GAIN_DEFAULT       13  // 0x0D
#define ADV_ACE_CHROMA_GAIN_DEFAULT     8
#define ADV_ACE_CHROMA_MAX_DEFAULT      8
#define ADV_ACE_GAMMA_GAIN_DEFAULT      8
#define ADV_ACE_RESPONSE_SPEED_DEFAULT  15  // 0x0F

// Packet templates for ACE parameters (use writeReg to fill value)
static const unsigned char ADV_ACE_Param[4] = {ADV_HEADER_0, ADV_HEADER_1, ADV_CMD_SOURCE, 0x00};
static const unsigned char ADV_ACE_Defaults[4] = {ADV_HEADER_0, ADV_HEADER_1, ADV_CMD_SOURCE, ADV_ACE_DEFAULTS};

// ====================================================================================
// Video Filter Commands
// Uses 'S' command with sub-commands 0xB0-0xB7 (shaping) and 0xB8-0xBD (comb control)
// Data byte in packet[4] contains the parameter value
// ====================================================================================

// Shaping Filter commands (0xB0-0xB6)
#define ADV_FILTER_Y_SHAPING     0xB0  // Set Y Shaping Filter for CVBS (0-31), value in packet[4]
#define ADV_FILTER_C_SHAPING     0xB1  // Set C Shaping Filter for CVBS (0-7), value in packet[4]
#define ADV_FILTER_WY_SHAPING    0xB2  // Set WY Shaping Filter for S-Video (0-31), value in packet[4]
#define ADV_FILTER_WY_OVERRIDE   0xB3  // Set WY Override (0=Auto, 1=Manual), value in packet[4]
#define ADV_FILTER_COMB_NTSC     0xB4  // Set Comb Filter NTSC bandwidth (0-3), value in packet[4]
#define ADV_FILTER_COMB_PAL      0xB5  // Set Comb Filter PAL bandwidth (0-3), value in packet[4]
#define ADV_VIDEO_FILTER_DEFAULTS 0xB7 // Reset ALL video filter parameters to defaults
#define ADV_COMB_LUMA_MODE_NTSC     0xB8  // Set NTSC Luma Mode (0,4-7), value in packet[4]
#define ADV_COMB_CHROMA_MODE_NTSC   0xB9  // Set NTSC Chroma Mode (0,4-7), value in packet[4]
#define ADV_COMB_CHROMA_TAPS_NTSC   0xBA  // Set NTSC Chroma Taps (0-3), value in packet[4]
#define ADV_COMB_LUMA_MODE_PAL      0xBB  // Set PAL Luma Mode (0,4-7), value in packet[4]
#define ADV_COMB_CHROMA_MODE_PAL    0xBC  // Set PAL Chroma Mode (0,4-7), value in packet[4]
#define ADV_COMB_CHROMA_TAPS_PAL    0xBD  // Set PAL Chroma Taps (0-3), value in packet[4]

// Video Filter parameter default values (from ADV7280 Main Register Map)
#define ADV_FILTER_Y_SHAPING_DEFAULT     1   // Auto Narrow
#define ADV_FILTER_C_SHAPING_DEFAULT     0   // Auto 1.5MHz
#define ADV_FILTER_WY_SHAPING_DEFAULT    19  // SVHS 18 (CCIR 601)
#define ADV_FILTER_WY_OVERRIDE_DEFAULT   1   // Manual
#define ADV_FILTER_COMB_NTSC_DEFAULT     0   // Narrow
#define ADV_FILTER_COMB_PAL_DEFAULT      1   // Medium

// Comb Control parameter default values (from current I2C_COMMANDS arrays)
// NTSC (0x38 = 0x80): CTAPSN=2, CCMN=0, YCMN=0
// PAL (0x39 = 0xC0): CTAPSP=3, CCMP=0, YCMP=0
#define ADV_COMB_LUMA_MODE_NTSC_DEFAULT     0   // Adaptive 3-line
#define ADV_COMB_CHROMA_MODE_NTSC_DEFAULT   0   // Adaptive
#define ADV_COMB_CHROMA_TAPS_NTSC_DEFAULT   2   // 5→3 lines
#define ADV_COMB_LUMA_MODE_PAL_DEFAULT      0   // Adaptive 5-line
#define ADV_COMB_CHROMA_MODE_PAL_DEFAULT    0   // Adaptive
#define ADV_COMB_CHROMA_TAPS_PAL_DEFAULT    3   // 5→4 lines

// Packet templates for Video Filter parameters
static const unsigned char ADV_VideoFilter_Param[4] = {ADV_HEADER_0, ADV_HEADER_1, ADV_CMD_SOURCE, 0x00};
static const unsigned char ADV_VideoFilter_Defaults[4] = {ADV_HEADER_0, ADV_HEADER_1, ADV_CMD_SOURCE, ADV_VIDEO_FILTER_DEFAULTS};

// ====================================================================================
// Video Format Mapping Table
// ====================================================================================

// Index to ADV7280 video format register value mapping
// Bits [7:4] = format select, Bits [3:0] = 0x4 (enable auto detection within format)
//
// 0 = Auto-detect all
// 1 = PAL B/G/H/I/D
// 2 = NTSC-M
// 3 = PAL-60
// 4 = NTSC-4.43
// 5 = NTSC-J
// 6 = PAL-N (with pedestal)
// 7 = PAL-M (without pedestal)
// 8 = PAL-M (with pedestal)
// 9 = PAL-Combination N
// 10 = PAL-Combination N (with pedestal)
// 11 = SECAM
static const uint8_t ADV_VideoFormats[12] = {
    0x04,  // 0: Auto
    0x84,  // 1: PAL
    0x54,  // 2: NTSC-M
    0x64,  // 3: PAL-60
    0x74,  // 4: NTSC-4.43
    0x44,  // 5: NTSC-J
    0x94,  // 6: PAL-N (wp)
    0xA4,  // 7: PAL-M (wop)
    0xB4,  // 8: PAL-M
    0xC4,  // 9: PAL-Cn
    0xD4,  // 10: PAL-Cn (wp)
    0xE4   // 11: SECAM
};

#define ADV_VIDEO_FORMAT_COUNT  (sizeof(ADV_VideoFormats) / sizeof(ADV_VideoFormats[0]))

// ====================================================================================
// ADVController Class
// ====================================================================================

/**
 * @class ADVController
 * @brief Handles UART packet transmission to the ADV controller
 *
 * Provides methods for various packet types:
 * - send(): Standard 4-byte command packets
 * - sendWithMode(): Commands with mode bits merged into data byte
 * - writeReg(): Register write commands (for BCSH)
 * - sendCustomI2C(): Custom I2C batch commands
 *
 * All methods handle random byte injection, end marker, and checksum automatically.
 */
// ====================================================================================
// NO-OP STUB (old GBSC has NO ADV / HC32F460 controller).
// Keeps the exact public API (constructor + send/sendWithMode/writeReg/sendCustomI2C)
// so every ADV_send* wrapper and all call sites still compile & link, but performs
// no UART traffic. All packet/constant definitions above remain intact.
// ====================================================================================
class ADVController {
public:
    explicit ADVController(HardwareSerial& serial = Serial) { (void)serial; }
    void send(const unsigned char* buff) { (void)buff; }
    void sendWithMode(const unsigned char* buff, uint8_t mode) { (void)buff; (void)mode; }
    void writeReg(const unsigned char* buff, unsigned char reg, unsigned char val) { (void)buff; (void)reg; (void)val; }
    void sendCustomI2C(const unsigned char* data, size_t size) { (void)data; (void)size; }
};

#endif // ADV_CONTROLLER_H_
