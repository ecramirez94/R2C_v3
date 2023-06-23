#include "configuration.h"
#include "mcp4151.h"
#include "pins.h"
#include "timers.h"

volatile bool send_count = false;
volatile uint8_t pulse_count = 0;
volatile uint8_t display_update_interval = 0;
volatile float timer_count = 0;

struct command
{
  char device;
  char number;
  char data[32];
  uint8_t data_len;
} command;

MCP4151 mcp4151;
Timers timers;

void setup()
{
  /* Enable/Disable peripherals 0 = Enabled, 1 = Disabled */
  PRR = (0 << PRTWI) | (0 << PRTIM2) | (0 << PRTIM0) | (0 << PRTIM1) | (0 << PRSPI) | (0 << PRUSART0) | (0 << PRADC);

  /* Initialize MCP4151 Digipot */
  mcp4151.begin();

  Serial.begin(115200);

  pinMode(TEST_PIN, OUTPUT);
  digitalWrite(TEST_PIN, LOW);

  timers.initHallPulseCounter();
  timers.initPulseCollectionTimer();
  timers.enableHallPulseCounter();
  timers.startDisplayUpdateTimer();
}


/*  =============================== Serial Event=================================
    serialEvent() automatically (quietly) called in the loop; in serial with
    State Machine's Queue.
    =============================================================================
*/
void serialEvent(void)
{
  String s = Serial.readStringUntil('\n');  // Read until newline char. The newline char is truncated.

  parseCommand(&command, s);

  /* The first two characters of the new command string direct the command data to the correct function.
      Then, the remaining payload and its length are given as arguments to their handling function.
      It is the responsibility of the individual functions to further unpack, convert, and otherwise
      interpret the data.
  */
  switch (command.device)
  {
    case dev_MCP4151:
      switch (command.number)
      {
        case MCP4151_WRITE_VALUE: /* B0<value> */
          mcp4151.setCount(command.data, command.data_len) ? Serial.println(F("ok")) : Serial.println(F("Invalid count"));
          break;
        case MCP4151_INCREMENT: /* "B1" */
          Serial.println("Count: " + String(mcp4151.increment()));
          break;
        case MCP4151_DECREMENT: /* "B2" */
          Serial.println("Count: " + String(mcp4151.decrement()));
          break;
      }
      break;
  }
}

void loop()
{
  if (send_count)
  {
    cli();
    Serial.println(timer_count, 0); // Truncate decimals from float
    send_count = false;
    sei();
  }
}


/* ===== Interrupt Service Routines ===== */
ISR(TIMER0_COMPA_vect)
{
  timers.disableHallPulseCounter();
  timer_count = (1.0 / ((TCNT1 >> 2) * 0.000004)) * 60;
  timers.enableHallPulseCounter();
}

ISR(TIMER2_COMPA_vect)
{
  display_update_interval++;

  if (display_update_interval >= 5)
  {
    send_count = true;
    display_update_interval = 0;
  }
}
/* ====================================== */

void parseCommand(struct command *c, String s)
{
  c->device = s[0];
  c->number = s[1];
  c->data_len = (s.length() - 2);

  for (uint8_t i = 0; i < c->data_len; i++)
    c->data[i] = s[i + 2];

  /* Manually add char array terminator */
  c->data[c->data_len] = '\0';
}
