#include <splash.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include "configuration.h"
#include "mcp4151.h"
#include "pins.h"
#include "screens.h"
#include "timers.h"

/* Control Modes */
#define LOCAL_CONTROL 0
#define REMOTE_CONTROL 1

/* Variables */
volatile bool send_count = false;
volatile uint8_t pulse_count = 0;
volatile uint8_t display_update_interval = 0;
volatile uint16_t timer_count = 0;

volatile uint16_t speed_accumulator = 0;
volatile uint8_t speed_count = 0;
uint16_t remote_speed_signal = 0;
uint16_t local_speed_signal = 0;

volatile uint16_t stall_buffer[STALL_BUFFER_SIZE];
uint8_t stall_count = 0;
bool stalled = false;

uint16_t router_load = 0;

volatile uint8_t control_mode = REMOTE_CONTROL;
volatile bool recover_oled = false;

volatile bool update_control_mode = false;
volatile bool watchdog_wait = false;

/* Structures */
struct command
{
  char device;
  char number;
  char data[32];
  uint8_t data_len;
} command;

MCP4151 mcp4151;
Timers timers;
Adafruit_SSD1306 *oled;

void setup()
{
  /* Enable/Disable peripherals 0 = Enabled, 1 = Disabled */
  PRR = (0 << PRTWI) | (0 << PRTIM2) | (0 << PRTIM0) | (0 << PRTIM1) | (0 << PRSPI) | (0 << PRUSART0) | (0 << PRADC);

  /* Initialize MCP4151 Digipot */
  mcp4151.begin();

  Serial.begin(115200);

  /* Configure I/O Pins */
  pinMode(CONTROL_MODE_SELECT_PIN, INPUT_PULLUP);
  pinMode(SPEED_CONTROL_REMOTE_PIN, INPUT);
  pinMode(SPEED_CONTROL_LOCAL_PIN, INPUT);
  pinMode(ROUTER_LOAD_PIN, INPUT);
  pinMode(TEST_PIN, OUTPUT);
  digitalWrite(TEST_PIN, LOW);

  oled = new Adafruit_SSD1306(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET_PIN);
  setupOLED();
  displaySplashScreen();
  watchdogDelay(2000);
  drawFrameOLED();
  updateRPMValue(0);

  /* Set Initial Conditions */
  control_mode = digitalRead(CONTROL_MODE_SELECT_PIN);
  updateControlMode(control_mode);

  PCICR = (1 << PCIE0); // Enable Pin Change Interrupts on pin bank: PCINT[7:0]
  PCMSK0 = (1 << PCINT0); // Enable Pin Change Interrupt on Arduino pin 8 (CONTROL_MODE_SELECT_PIN)

  /* Start Timers */
  timers.initHallPulseCounter();
  timers.initPulseCollectionTimer();
  timers.enableHallPulseCounter();
  timers.startDisplayUpdateTimer();

  enableWatchdog(2000);
  watchdogReset();
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
  /* ===== Recover OLED ===== */
  if (recover_oled)
  {
    recoverOLED();
    recover_oled = false;
  }

  /* ===== Update OLED ===== */
  if (update_control_mode)
  {
    updateControlMode(control_mode);
    update_control_mode = false;
  }

  if (send_count)
  {
    /* Update Router RPM */
    if (control_mode == REMOTE_CONTROL)
      mcp4151.setCount(map(remote_speed_signal, 0, 1023, 0, 255));
    else if (control_mode == LOCAL_CONTROL)
      mcp4151.setCount(map(local_speed_signal, 0, 1023, 0, 255));

    /* Update Router Load */
    updateLoadValue(router_load);

    /* Check for stall */
    stall_buffer[stall_count++] = timer_count;
    if (stall_count >= STALL_BUFFER_SIZE)
      stall_count = 0;

    for (uint8_t i = 0; i < STALL_BUFFER_SIZE; i++)
    {
      if (timer_count != stall_buffer[i])
      {
        stalled = false;
        break;
      } else if ((timer_count == stall_buffer[i]) && (router_load <= 10))
        /* To detect a stall (or if router was simply turned off), the entire stall buffer must be full of exactly the
            same number and the router load must be ~0 meaning the power is off.
        */
        stalled = true;
    }

    if (stalled)
    {
      updateRPMValue(0);
      stalled = false;
    }
    else
      updateRPMValue(timer_count);

    send_count = false;
  }

  /* ===== Measure Remote/Local Speed Control Signal ===== */
  if (control_mode == REMOTE_CONTROL)
    speed_accumulator += analogRead(SPEED_CONTROL_REMOTE_PIN);
  else if (control_mode == LOCAL_CONTROL)
    speed_accumulator += analogRead(SPEED_CONTROL_LOCAL_PIN);
  speed_count++;

  if (speed_count >= SPEED_AVERAGE_SIZE)
  {
    if (control_mode == REMOTE_CONTROL)
      remote_speed_signal = speed_accumulator / SPEED_AVERAGE_SIZE;
    else if (control_mode == LOCAL_CONTROL)
      local_speed_signal = speed_accumulator / SPEED_AVERAGE_SIZE;

    speed_accumulator = 0;
    speed_count = 0;
  }

  /* ===== Measure Load Current Signal ===== */
  router_load = analogRead(ROUTER_LOAD_PIN);

  watchdogReset();
}


/* ===== Interrupt Service Routines ===== */
ISR(TIMER0_COMPA_vect)
{
  cli();
  timers.disableHallPulseCounter();
  timer_count = uint16_t((1.0 / ((TCNT1 / 16) * 0.000004)) * 60);
  timers.enableHallPulseCounter();
  sei();
}

ISR(TIMER2_COMPA_vect)
{
  cli();
  display_update_interval++;

  if (display_update_interval >= 10)
  {
    send_count = true;
    display_update_interval = 0;
  }
  sei();
}

ISR(PCINT0_vect)
{
  cli();
  bool mode_select = digitalRead(CONTROL_MODE_SELECT_PIN);

  if (mode_select)
  {
    /* If CONTROL_MODE_SELECT_PIN was released (and pulled HIGH via internal pull-up resistor) */
    control_mode = REMOTE_CONTROL;
    speed_count = 0;
    speed_accumulator = 0;
    update_control_mode = true;
  }
  else
  {
    control_mode = LOCAL_CONTROL;
    speed_count = 0;
    speed_accumulator = 0;
    update_control_mode = true;
  }
  sei();
}

ISR(WDT_vect)
{
  digitalWrite(TEST_PIN, HIGH);

  watchdog_wait = true;

}
/* ====================================== */

void setupOLED(void)
{
  oled->begin(SSD1306_EXTERNALVCC, OLED_ADDRESS);
  oled->clearDisplay();
  oled->setTextSize(1);
  oled->setTextColor(WHITE);
}

void displaySplashScreen(void)
{
  /* Load splash screen into buffer */
  oled->drawXBitmap((oled->width()  - SPLASHSCREEN_WIDTH ) / 2,
                    (oled->height() - SPLASHSCREEN_HEIGHT) / 2,
                    sprouter_splashscreen, SPLASHSCREEN_WIDTH, SPLASHSCREEN_HEIGHT, 1);

  oled->display();
}

void recoverOLED(void)
{
  oled->~Adafruit_SSD1306();
  oled = new Adafruit_SSD1306(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET_PIN);
  setupOLED();
}

void drawFrameOLED(void)
{
  oled->clearDisplay();
  oled->drawRect(0, 0, oled->width(), oled->height(), WHITE);

  oled->setTextSize(2);
  oled->setCursor(113, 4);
  oled->print("R");
  oled->setCursor(113, 20);
  oled->print("P");
  oled->setCursor(113, 36);
  oled->print("M");

  oled->setTextSize(1);
  oled->drawLine(0, 53, 127, 53, WHITE);
  oled->setCursor(3, 55);
  oled->print("Load:");
  oled->drawRect(34, 55, 90, 7, WHITE);

  oled->display();
}

void updateControlMode(uint8_t mode)
{
  oled->setTextSize(1);
  /* Clear old text before printing new */
  oled->fillRect(1, 4, 100, 7, BLACK);

  if (mode == REMOTE_CONTROL)
  {
    displayCenterOfRow("REMOTE", 4);
  }

  else if (mode == LOCAL_CONTROL)
  {
    displayCenterOfRow("LOCAL", 4);
  }
}

void updateRPMValue(uint16_t rpm)
{
  /* Each character is 15 pixels wide plus 3 in between. Total pixels per character equals 18 */
  oled->setTextSize(3);
  oled->fillRect(14, 16, 95, 21, BLACK);

  if (rpm >= 10000)
    oled->setCursor(14, 16);
  else if (rpm >= 1000)
    oled->setCursor(32, 16);
  else if (rpm >= 100)
    oled->setCursor(50, 16);
  else if (rpm >= 10)
    oled->setCursor(78, 16);
  else if (rpm >= 0)
    oled->setCursor(86, 16);

  oled->print(rpm);

  oled->display();
}

void updateLoadValue(uint16_t load_signal)
{
  uint8_t load = map(load_signal, 0, 1023, 0, 88);

  oled->fillRect(35, 56, 88, 5, BLACK);
  oled->fillRect(35, 56, load, 5, WHITE);
}

void displayCenterOfRow(String text, uint8_t row)
{
  uint8_t len = calcTextLength(text);
  uint8_t x_pos = (OLED_WIDTH - len) / 2;

  // display on horizontal center
  oled->setCursor(x_pos, row);
  oled->println(text); // text to display
  oled->display();
}

uint8_t calcTextLength(String text)
{
  int16_t x1;
  int16_t y1;
  uint16_t width = 0;
  uint16_t height = 0;

  oled->getTextBounds(text, 0, 0, &x1, &y1, &width, &height);

  return width;
}

/* Watchdog Timer Function
  - Time-out given in milliseconds according to Table 11-2
  - See section 11.8 "Watchdog Timer" for more information
  - Only set to interrupt mode.
  - In conjunction with a while loop, can be used as a blocking delay function.
*/
void enableWatchdog(uint16_t time_out)
{
  uint8_t control_bits = 0x00;
  uint8_t prescale = 0x00;

  /* Set config bits */
  /* Clear interrupt flag (if set) | Enable interrupt mode | Clear Change Enable | Disable reset mode | Set prescaler to default */
  control_bits = (1 << WDIF) | (1 << WDIE) | (0 << WDP3) | (0 << WDCE) | (0 << WDE) | (0 << WDP2) | (0 << WDP1) | (0 << WDP0);

  /* Determine prescale bits */
  switch (time_out)
  {
    case 16:
      prescale = 0x00;
      break;

    case 32:
      prescale = 0x01;
      break;

    case 64:
      prescale = 0x02;
      break;

    case 125:
      prescale = 0x03;
      break;

    case 250:
      prescale = 0x04;
      break;

    case 500:
      prescale = 0x05;
      break;

    case 1000:
      prescale = 0x06;
      break;

    case 2000:
      prescale = 0x07;
      break;

    case 4000:
      prescale = 0x20;
      break;

    case 8000:
      prescale = 0x21;
      break;
  }

  /* Combine bits */
  control_bits |= prescale;

  /* Write bits to Watchdog Timer */
  cli();
  MCUSR &= ~(1 << WDRF);
  watchdogReset();

  /* Start timed sequence */
  WDTCSR |= (1 << WDCE) | (1 << WDE);
  /* Write new configuration in one operation */
  WDTCSR = control_bits;

  sei();
}

void watchdogReset(void)
{
  asm("WDR \n");
}

void watchdogDelay(uint16_t milliseconds)
{
  /* Delay given in milliseconds according to Table 11-2. */

  /* Configure WDT */
  enableWatchdog(milliseconds);
  watchdogReset();

  /* Blocking delay */
  while (!watchdog_wait) {
    asm("nop \n");
  }

  /* Disable Watchdog when complete */
  disableWatchdog();

  /* Reset boolean */
  watchdog_wait = false;
  recover_oled = false;
}

void disableWatchdog(void)
{
  cli();
  watchdogReset();

  MCUSR &= ~(1 << WDRF);
  WDTCSR |= (1 << WDCE) | (1 << WDE);
  WDTCSR = 0x00;

  sei();
}

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
