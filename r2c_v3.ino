#include <splash.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include "configuration.h"
#include "mcp4151.h"
#include "pins.h"
#include "timers.h"

volatile bool send_count = false;
volatile uint8_t pulse_count = 0;
volatile uint8_t display_update_interval = 0;
volatile uint16_t timer_count = 0;


uint16_t speed_accumulator = 0;
uint8_t speed_count = 0;
uint16_t remote_speed_signal = 0;
uint16_t local_speed_signal = 0;

volatile uint16_t stall_buffer[STALL_BUFFER_SIZE];
uint8_t stall_count = 0;
bool stalled = false;

uint16_t router_load = 0;

struct command
{
  char device;
  char number;
  char data[32];
  uint8_t data_len;
} command;

MCP4151 mcp4151;
Timers timers;
Adafruit_SSD1306 oled(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET_PIN);

void setup()
{
  /* Enable/Disable peripherals 0 = Enabled, 1 = Disabled */
  PRR = (0 << PRTWI) | (0 << PRTIM2) | (0 << PRTIM0) | (0 << PRTIM1) | (0 << PRSPI) | (0 << PRUSART0) | (0 << PRADC);

  /* Initialize MCP4151 Digipot */
  mcp4151.begin();

  Serial.begin(115200);

  setupOLED();
  drawFrameOLED();
  updateRPMValue(0);

  pinMode(ROUTER_SPEED_CONTROL_PIN, INPUT);
  pinMode(ROUTER_LOAD_PIN, INPUT);
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
  /* ===== Update Display ===== */
  if (send_count)
  {
    /* Update Router RPM */
    mcp4151.setCount(map(remote_speed_signal, 0, 1023, 0, 255));

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
      } else if (timer_count < 8500)
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

  /* ===== Measure Remote Speed Control Signal ===== */
  speed_accumulator += analogRead(ROUTER_SPEED_CONTROL_PIN);
  speed_count++;

  if (speed_count >= SPEED_AVERAGE_SIZE)
  {
    remote_speed_signal = speed_accumulator / SPEED_AVERAGE_SIZE;

    speed_accumulator = 0;
    speed_count = 0;
  }

  /* ===== Measure Local Speed Control Signal ===== */



  /* ===== Measure Load Current Signal ===== */
  router_load = analogRead(ROUTER_LOAD_PIN);
}


/* ===== Interrupt Service Routines ===== */
ISR(TIMER0_COMPA_vect)
{
  timers.disableHallPulseCounter();
  timer_count = uint16_t((1.0 / ((TCNT1 / 16) * 0.000004)) * 60);
  timers.enableHallPulseCounter();
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
/* ====================================== */

void setupOLED(void)
{
  oled.begin(SSD1306_EXTERNALVCC, OLED_ADDRESS);
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(WHITE);
  oled.display();
}

void drawFrameOLED(void)
{
  oled.drawRect(0, 0, oled.width(), oled.height(), WHITE);

  oled.setTextSize(2);
  oled.setCursor(113, 4);
  oled.print("R");
  oled.setCursor(113, 20);
  oled.print("P");
  oled.setCursor(113, 36);
  oled.print("M");

  oled.setTextSize(1);
  oled.drawLine(0, 53, 127, 53, WHITE);
  oled.setCursor(3, 55);
  oled.print("Load:");
  oled.drawRect(34, 55, 90, 7, WHITE);

  oled.display();
}

void updateRPMValue(uint16_t rpm)
{
  /* Each character is 15 pixels wide plus 3 in between. Total pixels per character equals 18 */
  oled.setTextSize(3);
  oled.fillRect(14, 16, 95, 21, BLACK);

  if (rpm >= 10000)
    oled.setCursor(14, 16);
  else if (rpm >= 1000)
    oled.setCursor(32, 16);
  else if (rpm >= 100)
    oled.setCursor(50, 16);
  else if (rpm >= 10)
    oled.setCursor(78, 16);
  else if (rpm >= 0)
    oled.setCursor(86, 16);

  oled.print(rpm);

  oled.display();
}

void updateLoadValue(uint16_t load_signal)
{
  uint8_t load = map(load_signal, 0, 1023, 0, 88);
  
  oled.fillRect(35, 56, 88, 5, BLACK);
  oled.fillRect(35, 56, load, 5, WHITE);
}

void displayCenterOfRow(String text, uint8_t row)
{
  uint8_t len = calcTextLength(text);

  // display on horizontal and vertical center
  oled.setCursor((OLED_WIDTH - len) / 2, row);
  oled.println(text); // text to display
  oled.display();
}

uint8_t calcTextLength(String text)
{
  int16_t x1;
  int16_t y1;
  uint16_t width = 0;
  uint16_t height = 0;

  oled.getTextBounds(text, 0, 0, &x1, &y1, &width, &height);

  return width;
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
