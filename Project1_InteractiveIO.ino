/*
 * Project 1 — Interactive I/O
 * Button debounce + LED state machine + status on UART and a 16x2 LCD
 *
 * Board: Arduino UNO R3 (ELEGOO compatible). Portable to ESP32 later.
 * Goal: make hardware do something deterministically, with clean debouncing,
 *       non-blocking timing, and status visible on two independent outputs.
 *
 * Wiring (UNO):
 *   - Pushbutton: one leg to pin 7, other leg to GND. Internal pull-up, so the
 *     pin reads HIGH when released and LOW when pressed.
 *   - LED: pin 13, ~220-330R in series to GND (onboard LED also works).
 *   - LCD: HD44780 16x2 in 4-bit mode, write-only (RW tied to GND).
 *
 *       LCD pin   Name   Arduino
 *          1      VSS    GND
 *          2      VDD    5V
 *          3      VO     10k potentiometer wiper
 *          4      RS     12
 *          5      RW     GND
 *          6      E      11
 *        7-10     D0-D3  not connected (4-bit mode)
 *         11      D4     5
 *         12      D5     4
 *         13      D6     3
 *         14      D7     2
 *         15      A      5V through 220R
 *         16      K      GND
 *
 *     NOTE: the button moved from pin 2 to pin 7 because LCD D7 now occupies
 *     pin 2. Leaving it on 2 makes digitalRead() sample an LCD data line.
 *
 * What it does:
 *   - Reads the button with a debounce routine (no spurious toggles).
 *   - Each clean press cycles an LED state machine: OFF -> ON -> BLINK -> OFF.
 *   - Prints a banner on boot; logs every state change to UART and the LCD.
 *
 * Design note — why the LCD is written only on state change:
 *   The HD44780 is slow and LiquidCrystal inserts blocking delays around its
 *   commands. Calling lcd.print() every loop() pass would stall the loop
 *   thousands of times per second and starve the button polling that debounce
 *   depends on. printState() is called only from advanceState(), which fires
 *   only on a clean press, so the write rate is bounded by human input.
 *
 * Design note — fixed-width state strings instead of lcd.clear():
 *   Overwriting "BLINK" with "ON" would leave "ONINK" on the row. clear() fixes
 *   it but causes a visible flicker on every press. Padding each name to 5
 *   characters overwrites the leftovers with spaces in the same single write —
 *   no flicker, no extra command, and the row width is known at compile time.
 */

#include <LiquidCrystal.h>

// RS, E, D4, D5, D6, D7
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

const uint8_t PIN_BUTTON = 7;    // moved off pin 2: LCD D7 lives there now
const uint8_t PIN_LED    = 13;

const uint8_t LCD_COLS = 16;
const uint8_t LCD_ROWS = 2;

// Debounce settings
const unsigned long DEBOUNCE_MS = 30;

// LED state machine
enum LedState { LED_OFF, LED_ON, LED_BLINK };
LedState ledState = LED_OFF;

// Button debounce tracking
int           lastReading    = HIGH;   // pull-up idle = HIGH
int           stableState    = HIGH;
unsigned long lastChangeTime = 0;

// Blink timing (non-blocking)
const unsigned long BLINK_MS = 250;
unsigned long lastBlinkTime = 0;
bool          blinkOn       = false;

// Padded to a fixed 5 chars so a shorter name fully covers a longer one.
const char* stateName(LedState s) {
  switch (s) {
    case LED_OFF:   return "OFF  ";
    case LED_ON:    return "ON   ";
    case LED_BLINK: return "BLINK";
  }
  return "?????";
}

void printState() {
  Serial.print(F("[state] LED -> "));
  Serial.println(stateName(ledState));

  lcd.setCursor(0, 1);
  lcd.print("LED: ");
  lcd.print(stateName(ledState));
}

void advanceState() {
  ledState = static_cast<LedState>((ledState + 1) % 3);
  printState();
}

void setup() {
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);

  lcd.begin(LCD_COLS, LCD_ROWS);
  lcd.setCursor(0, 0);
  lcd.print(F("Project 1  I/O"));

  Serial.begin(9600);
  while (!Serial) { ; }  // wait for serial (harmless on UNO)
  Serial.println(F("hello - Project 1: Interactive I/O"));
  Serial.println(F("Press the button to cycle LED: OFF -> ON -> BLINK"));
  printState();
}

void loop() {
  // --- Debounce the button ---
  int reading = digitalRead(PIN_BUTTON);
  if (reading != lastReading) {
    lastChangeTime = millis();          // input changed; restart the timer
    lastReading = reading;
  }
  if ((millis() - lastChangeTime) > DEBOUNCE_MS) {
    // reading has been stable long enough to trust it
    if (reading != stableState) {
      stableState = reading;
      if (stableState == LOW) {         // pressed (pull-up -> LOW on press)
        advanceState();
      }
    }
  }

  // --- Drive the LED based on state ---
  switch (ledState) {
    case LED_OFF:
      digitalWrite(PIN_LED, LOW);
      break;
    case LED_ON:
      digitalWrite(PIN_LED, HIGH);
      break;
    case LED_BLINK:
      if (millis() - lastBlinkTime >= BLINK_MS) {
        lastBlinkTime = millis();
        blinkOn = !blinkOn;
        digitalWrite(PIN_LED, blinkOn ? HIGH : LOW);
      }
      break;
  }
}
