/*
 * sim_wire.h - the screen twin's only interface: a virtual SPI link over TCP.
 *
 * Every transfer is one frame: [kind u8][len u32 LE][len bytes]
 *
 *   host -> screen
 *     SIM_WIRE_CMD        command byte, D/C pin low (len 1)
 *     SIM_WIRE_DATA       data bytes, D/C pin high
 *     SIM_WIRE_BACKLIGHT  backlight PWM 0..100 (len 1)
 *   screen -> host
 *     SIM_WIRE_BUTTONS    button mask held down (len 1)   keys 1 2 3
 *     SIM_WIRE_LIGHT      ambient light 0..100, 255 = no sensor (len 1)   key L
 *
 * CMD / DATA bytes are exactly what would go down the wire to a real
 * ILI9341.
 */
#ifndef SIM_WIRE_H
#define SIM_WIRE_H

#define SIM_WIRE_PORT       9341
#define SIM_WIRE_CMD        0u
#define SIM_WIRE_DATA       1u
#define SIM_WIRE_BACKLIGHT  2u
#define SIM_WIRE_BUTTONS    0x80u
#define SIM_WIRE_LIGHT      0x81u
#define SIM_WIRE_HEADER     5u

#endif /* SIM_WIRE_H */
