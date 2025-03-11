# hard fault :: a Raspberry Pi Pico 2 (RP2350) demo

a silly 5 days production for the DiHalt 2025 Winter HiEnd demo compo :)

https://demozoo.org/productions/365233/ :: https://www.youtube.com/watch?v=jIQhSY1BZxQ

everything runs at 320x240 RGB555 (16 bits per pixel) @ 60hz, pixel doubled and displayed using HSTX unit via DVI output. 

first core runs the demo, seconds keeps the video and music running.
the chip itself is *under*clocked to 126MHz to keep the 25.2MHz pixel clock.

To run this prod, you'll need:

- a RP2350 microcontroller board (should work with both RP2350A and RP2350B)
- an DVI/HDMI breakout board and a display capable of receiving DVI 640x480 60hz signal
  - currently, Murmulator 2 and Pico-DVI-Sock boards are supported - adding other HSTX->DVI mappings is pretty straightforward - check `core1.cpp` 
- and an I2S DAC to hear the music :)  (tested with PCM5102 board)

## quick pinout:

### I2S DAC:

| GPIO pin | Function |
| -------- | -------- |
| 9        | DATA     |
| 10       | BCLK     |
| 11       | LRCLK    |

### DVI/HDMI video output:

| Function | Murmulator 2 GPIO | Pico-DVI-Sock pin |
| -------- | ----------------- | ----------------- |
|  Clock-           | 12 | 15 |
|  Clock+           | 13 | 14 |
|  Lane0-           | 14 | 13 |
|  Lane0+           | 15 | 12 |
|  Lane1-           | 16 | 16 |
|  Lane1+           | 17 | 17 |
|  Lane2-           | 18 | 18 |
|  Lane2+           | 19 | 19 |

 and a debug UART at GPIO 0 and 1

## Build instructions

Install Pico SDK (tested with 2.1.0), configure via CMake and run the generated makefile, or use a vscode extension like i did :)



--artёmka (aka wbcbz7) o6.o1 - 11.o3.2o25