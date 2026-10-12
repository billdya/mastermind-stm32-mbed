# mastermind-stm32-mbed
# Electronic Mastermind Game

Embedded version of Mastermind on an STM32 Nucleo-F401RE, written in C++ with Mbed.

![Demo](images/demo.gif)

## Features
- 4-colour code guessing game with 10 attempts
- Finite state machine controlling game flow
- Software debouncing for push buttons
- Colour matching algorithm (correct colour/position and correct colour only)
- LCD, multiplexed 7-segment display and onboard LED show status,
  remaining attempts and result

## Hardware
- STM32 Nucleo-F401RE
- LCD display, 7-segment display, push buttons, LEDs
- (add a wiring table or circuit diagram)

## How it works
Short explanation of the state machine (a diagram helps) and the matching algorithm.

## Build and run
1. Open in Mbed Studio / Keil MDK
2. Select NUCLEO-F401RE
3. Compile and flash

## What I learned
2-3 lines on challenges, e.g. debouncing or display multiplexing timing.
