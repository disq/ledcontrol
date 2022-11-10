# Pico LED Control

Simple LED Control project for Raspberry Pi Pico.

- [Hardware](#hardware)
- [Before you start](#before-you-start)
- [Prepare build environment](#prepare-build-environment)
- [Grab the Pimoroni libraries](#grab-the-pimoroni-libraries)
- [Clone this repository](#clone-this-repository)
- [Prepare project environment](#prepare-project-environment)
- [Build](#build)
- [Flash](#flash)

## Hardware

- Raspberry Pi [Pico](https://shop.pimoroni.com/products/raspberry-pi-pico) or [Pico W](https://shop.pimoroni.com/products/raspberry-pi-pico-w)
- Pimoroni [RGB Encoder Breakout](https://shop.pimoroni.com/products/rgb-encoder-breakout)
- A [WS2812 LED strip](https://shop.pimoroni.com/collections/components?tags=LED%20Strip) or a bunch of [NeoPixels](https://www.adafruit.com/category/168)
- Read [the uberguide](https://learn.adafruit.com/adafruit-neopixel-uberguide)
- Three buttons, connecting various pins to GND when pressed (active low)

### Connections

- WS2812: Data on GP28. Power with 5V and GND.
- RGB Encoder Breakout: Connect GP4,GP5 for I2C(SDA,SCL) and GP3 for interrupt. [Breakout Garden](https://shop.pimoroni.com/products/pico-breakout-garden-base) can be used.
- Button "A" to GP11. Changes encoder mode.
- Button "B" to GP12. Enables cycling.
- Button "C" to GP13. Resets effects.
- Buttons are active-low.

## Before you start

It's easier if you make a `pico` directory or similar in which you keep the SDK, Pimoroni Libraries and your projects alongside each other. This makes it easier to include libraries.

## Prepare build environment

Install build requirements:

```bash
sudo apt update
sudo apt install cmake gcc-arm-none-eabi build-essential
```

And the Pico SDK:

```
git clone https://github.com/raspberrypi/pico-sdk
cd pico-sdk
git submodule update --init
export PICO_SDK_PATH=`pwd`
cd ../
```

The `PICO_SDK_PATH` set above will only last the duration of your session.

You should ensure your `PICO_SDK_PATH` environment variable is set by `~/.profile`:

```
export PICO_SDK_PATH="/path/to/pico-sdk"
```

## Grab the Pimoroni libraries

```
git clone https://github.com/pimoroni/pimoroni-pico
cd pimoroni-pico
git submodule update --init
cd ..
```

## Clone this repository

```
git clone https://github.com/disq/ledcontrol
cd ledcontrol
git submodule update --init
```

## Prepare project environment

In `ledcontrol` directory:

```
mkdir build
cd build
cmake ..
```

## Build

```
make ledcontrol
```

## Flash

Hold down the BOOTSEL button on the Pico and plug it into your computer. The Pico will appear as a USB drive called `RPI-RP2`. Copy the `ledcontrol.uf2` file to the root of the drive.

for macOS Ventura, try:
```
/bin/cp -X ledcontrol.uf2 /Volumes/RPI-RP2/
```
