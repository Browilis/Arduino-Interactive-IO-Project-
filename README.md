# Project 1 — Interactive I/O

Button debounce + LED state machine, with live status on both a UART console and
a 16x2 character LCD, running on real hardware.

> **Why this project:** proves I can make a microcontroller do something
> deterministically — clean input handling (no switch bounce), a simple state
> machine, non-blocking timing, and status reported to two independent output
> devices with very different timing characteristics.

![Arduino UNO, breadboard and 16x2 HD44780 LCD wired in 4-bit parallel mode. The
display reads "Project 1 I/O / LED: BLINK" — the state machine running on
hardware.](wiring.jpg)

## What it does

- Boots and prints a `hello` banner + instructions over UART (9600 baud), and a
  static banner on the LCD.
- Reads a pushbutton with a **software debounce** routine (ignores contact bounce).
- Each clean press cycles an LED state machine: **OFF → ON → BLINK → OFF**.
- Reports every state change to **both** the serial console and the LCD.
- Blink is **non-blocking** (`millis()` timing, no `delay()`), so the button stays
  responsive while the LED blinks.

## Hardware

- Arduino UNO R3 (ELEGOO compatible) — from the ELEGOO Super Starter Kit.
- 1x pushbutton, 1x LED + ~220–330Ω resistor (onboard LED on pin 13 also works).
- 1x HD44780-compatible 16x2 character LCD, driven in **4-bit parallel mode**.
- 1x 10kΩ potentiometer (LCD contrast), 1x 220Ω resistor (LCD backlight).
- Breadboard + jumper wires.

## Wiring

| Component | Arduino pin | Notes |
|-----------|-------------|-------|
| Button    | D7 → GND | `INPUT_PULLUP`; reads HIGH idle, LOW pressed |
| LED (+)   | D13 | Series resistor to GND; onboard LED also on D13 |
| LCD RS    | D12 | Register select |
| LCD E     | D11 | Enable |
| LCD D4    | D5  | 4-bit data bus |
| LCD D5    | D4  | |
| LCD D6    | D3  | |
| LCD D7    | D2  | |
| LCD RW    | GND | Tied low — display is write-only |
| LCD VSS / K | GND | Ground and backlight cathode |
| LCD VDD   | 5V | |
| LCD A     | 5V through 220Ω | Backlight anode |
| LCD VO    | 10kΩ pot wiper | Contrast; pot outer legs to 5V and GND |
| LCD D0–D3 | *not connected* | Unused in 4-bit mode |

Constructor matching this map — argument order is `RS, E, D4, D5, D6, D7`:

```cpp
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);
```

## Build & run

1. Open `Project1_InteractiveIO.ino` in the Arduino IDE.
2. Select **Tools → Board → Arduino Uno** and the correct serial port.
3. Upload.
4. Open **Tools → Serial Monitor**, set baud to **9600**.
5. Press the button; the LED state cycles in the monitor and on the LCD.

## Design decisions

**The LCD is written only on state change, never inside the polling loop.**
The HD44780 is slow, and `LiquidCrystal` inserts blocking delays around its
commands — each character costs tens of microseconds. Calling `lcd.print()` on
every `loop()` iteration would stall the loop thousands of times per second,
which directly starves the button polling that the debounce window depends on.
`printState()` is called only from `advanceState()`, which fires only on a clean
press, so LCD write frequency is bounded by human input rather than clock speed.
This is the same reasoning behind using `millis()` instead of `delay()` for the
blink, applied to a second peripheral.

**Fixed-width state strings instead of `lcd.clear()`.**
Overwriting `BLINK` with `ON` leaves `ONINK` on the row. `lcd.clear()` fixes it
but flickers the whole display on every press. Padding each state name to a fixed
5 characters overwrites the leftovers with spaces in the same single write — no
flicker, no extra command, and the field width is known at compile time. The
tradeoff is that the padding is now a correctness constraint the strings have to
maintain, which the `stateName()` helper localizes to one place.

**Button moved from D2 to D7.**
LCD D7 occupies Arduino pin 2 in this wiring, so leaving the button there would
have made `digitalRead()` sample an active LCD data line. Caught while assigning
the LCD pin map rather than at the bench — reading your own pin assignments for
conflicts before powering up is cheaper than debugging a phantom input.

## Debugging stories

**The display that powered up but never displayed anything.**
I expected the LCD to print its banner the moment the sketch ran. The backlight
came on and the screen stayed completely blank — no characters, and not even the
row of solid blocks that a live-but-mis-contrasted HD44780 shows. Serial output
confirmed the firmware was running and reaching `printState()`, which moved the
fault off the code and onto the display side. Re-reading the wiring notes for the
VO pin, I found I had tied it straight to 5 V. VO sets the contrast bias and
expects a voltage *between* the rails, not a rail itself; driven to 5 V the
characters and the background sit at the same apparent level, so a working
display looks like a dead one. I added a 10 kΩ potentiometer as a divider across
5 V and GND with the wiper on VO, swept it end to end, and the banner appeared
partway through the range.

The takeaway I carried forward: "not displaying" and "not running" are different
failures, and the serial console is the cheapest instrument for telling them
apart. I spent an evening suspecting my code for a wiring fault the datasheet
describes in one line.

**The I2C scanner that found nothing.**
I expected an I2C bus scan to report an address for the LCD. It found no devices
at all. Hypothesis: the display is a parallel HD44780, not an I2C module, so it
has no bus address to find. Confirmed by counting the pins — 16 parallel lines —
and by the presence of a contrast potentiometer in the circuit, which an I2C
backpack would handle internally. The scanner was working correctly; my model of
the part was wrong.

The takeaway I carried forward: when an instrument reports nothing, check the
assumption that the thing being measured is the kind of thing the instrument can
see, before assuming the instrument is broken.

## Concepts practiced

- Timer-based polling debounce (non-blocking, no interrupts, no `volatile`)
- Non-blocking timing with `millis()` vs. `delay()`
- A small `enum` state machine
- UART / serial as a debugging instrument
- Driving an HD44780 in 4-bit parallel mode, and why D0–D3 stay unconnected
- Separating a peripheral's analog input (contrast) from its digital interface,
  and biasing it with a potentiometer divider rather than a supply rail
- Budgeting a slow peripheral's writes so they don't starve a time-sensitive loop

## Next

Port this to the ESP32 when it arrives (3.3 V logic, different pin numbers) — a
good first "read the datasheet / pinout" rep.
