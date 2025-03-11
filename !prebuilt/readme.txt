. hard fault
. a raspberry pi pico 2 craptro
. 0t0mata labs 2o25
. at dihalt winter 2025

you wouldn't believe how powerful Cortex-M33 is
enjoy 1995ish demo vibes :D

everything runs at 320x240 RGB555 (16 bits per pixel) @ 60hz,
pixel doubled and displayed using HSTX unit via DVI output
first core runs the demo, seconds keeps the video and music running
the chip itself is _under_clocked to 126MHz to keep the 25.2MHz pixel clock

to run this prod, you'll need:
- a RP2350 microcontroller board (should work with both RP2350A and RP2350B)
- an DVI/HDMI breakout board and a display capable of receiving DVI 640x480 60hz signal
  - currently, Murmulator 2 and Pico-DVI-Sock boards are supported
- and an I2S DAC to hear the music :)

quick pinout:
  I2S DAC:
    GPIO  9 - DATA
    GPIO 10 - BCLK
    GPIO 11 - LRCLK
  DVI/HDMI:
    pin name   Pico-DVI-Sock   Murmulator 2
    Clock-          15             12
    Clock+          14             13
    Lane0-          13             14
    Lane0+          12             15
    Lane1-          19             16
    Lane1+          18             17
    Lane2-          17             18
    Lane2+          16             19
  
  + debug UART at GPIO0/1

all code, "textures" and music by artёmka (still pretty much known as wbcbz7)
end pic by grongy

greets to everyone at the party :)
special regards to Flux, polpo and murmulator dev group @ telegram

.06.01.2025

p.s. some bugs seem to be left unfixed (party version lol)