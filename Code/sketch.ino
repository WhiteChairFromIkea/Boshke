// Perfect timer calculator: http://www.8bit-era.cz/arduino-timer-interrupts-calculator.html

#define PIN_RELAY 5
#define PIN_PUSHBTN 2
#define PIN_LED_EXT 4 // Use different pin for led, because external led is driven high at arduino bootloader for some time

#define RELAY_CNT 100 // relay on duration, ms
#define MAX_HOURS 24  // temporaly not used
#define DEBOUNCE_CYCLES 5
#define LED_PERIOD_FINISHED_10MS 50 // 100 = 1s
#define LED_DURARION_PAUSE_10MS 100 // kiek 10ms pauze daryti mirksint valandas
#define LED_DURARION_HOUR_INDICATE_ON_10MS 20 // kiek 10ms pauze daryti mirksint valandas
#define LED_DURARION_HOUR_INDICATE_OFF_10MS 20 // kiek 10ms pauze daryti mirksint valandas
#define COEF_HOURS_TO_10MS 100UL * 60UL * 60UL // 1h = 10ms * 100ms *60s * 60m


enum states
{
  STATE_0_WAITING_INITIAL_PRESS,
  STATE_1_COUNTING_DOWN,
  STATE_2_RELAY_ENERGIZED,
  STATE_3_RELAY_FINISHED_IDLE
};

enum ledStates
{
  STATE_LED_OFF_PAUSE,
  STATE_LED_ON_HOUR,
  STATE_LED_OFF_HOUR
};

volatile uint8_t flag_10ms = 0; // updated in IRQ every 10ms

uint8_t button_down = 0;
uint8_t led_blink_count_left = 0; // 0 .. 24 hours can be set
uint32_t downCounter_10ms = 0; // Hours left, quantized in 10 ms
uint8_t downCounter_Relay = RELAY_CNT;
uint8_t downCounter_Led = 1; // Instant led feedback about about hours count
uint8_t ledCounter_Finished = 0;

enum states state = STATE_0_WAITING_INITIAL_PRESS;
enum ledStates ledState = STATE_LED_OFF_PAUSE;

// Check button state and set the button_down variable if a debounced button down press is detected. Call this function about 100 times per second.
static inline void debounce(void)
{
  static uint8_t debounce_count = 0; // Counter for number of equal states
  static uint8_t btn_state = 0;      // Keeps track of current (debounced) state

  uint8_t btn_current_state = !digitalRead(PIN_PUSHBTN); //(~BUTTON_PIN & BUTTON_MASK) != 0; // Check if button is high or low for the moment

  if (btn_current_state != btn_state)
  {
    // Button state is about to be changed, increase counter
    debounce_count++;
    if (debounce_count >= DEBOUNCE_CYCLES)
    {
      // The button have not bounced for four checks, change state
      btn_state = btn_current_state;
      // If the button was pressed (not released), tell main so
      if (btn_current_state != 0)
      {
        button_down = 1;
      }
      debounce_count = 0;
    }
  }
  else
  {
    // Reset counter
    debounce_count = 0;
  }
}

uint32_t div_ceil(uint32_t x, uint32_t y) {
  return (x + y - 1) / y;
}

void relayOn(void) {
  digitalWrite(PIN_RELAY, HIGH);
};

void relayOff(void)
{
  digitalWrite(PIN_RELAY, LOW);
};

void ledOn(void)
{
  digitalWrite(LED_BUILTIN, HIGH);
  digitalWrite(PIN_LED_EXT, HIGH);
};

void ledOff(void)
{
  digitalWrite(LED_BUILTIN, LOW);
  digitalWrite(PIN_LED_EXT, LOW);
};

void toggle_led(void) {
  digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  digitalWrite(PIN_LED_EXT, !digitalRead(PIN_LED_EXT));
}

uint32_t get10msTicksFromHours(uint8_t hours) {
  return ( hours * COEF_HOURS_TO_10MS );
}

uint8_t getHoursFrom10msTicks_Ceil(uint32_t ticks_10ms) {
  return (uint8_t)div_ceil( ticks_10ms, (COEF_HOURS_TO_10MS ));
}

void addOneHourAndReset(void)
{
  // Do nothing if relay cycle has been started
  if (state > STATE_1_COUNTING_DOWN)
  {
    return;
  }

  // Special case: check if first hour is to be added
  if (state == STATE_0_WAITING_INITIAL_PRESS)
  {
    downCounter_10ms = get10msTicksFromHours(1);
    return;
  }

  uint8_t hoursAlreadySet = (uint8_t)getHoursFrom10msTicks_Ceil( downCounter_10ms );

  // Check if user tried to set more than MAX_HOUR
  if ( hoursAlreadySet == MAX_HOURS )
  {
    downCounter_10ms = get10msTicksFromHours(1); // Set to 1 hour if overflow
    return;
  }

  // Else, increment and reset hour count
  downCounter_10ms = get10msTicksFromHours( hoursAlreadySet + 1 );
}

void ledBlink_Finished(void)
{
  if (ledCounter_Finished == LED_PERIOD_FINISHED_10MS)
  {
    ledCounter_Finished = 0;
    toggle_led();
  }
  else
  {
    ledCounter_Finished++;
  }
}

void ledBlink_HoursLeft(void)
{
  // Todo
  // Blink how many hours left
  // One 0.5s blink mean 6 hours, 0.3s blink mean 1Hour;
  // ... -> 3 hours;
  // -.. -> 8 hours.

  switch (ledState)
  {
    case STATE_LED_ON_HOUR:
      {
        downCounter_Led--;
        if ( downCounter_Led == 0 )
        {
          ledOff();
          downCounter_Led = LED_DURARION_HOUR_INDICATE_OFF_10MS;
          ledState = STATE_LED_OFF_HOUR;
        }
      }
      break;

    case STATE_LED_OFF_HOUR:
      {
        downCounter_Led--;
        if ( downCounter_Led == 0)
        {
          led_blink_count_left--;

          if ( led_blink_count_left == 0 )
          {
            downCounter_Led = LED_DURARION_PAUSE_10MS;
            ledState = STATE_LED_OFF_PAUSE;
          }
          else
          {
            downCounter_Led = LED_DURARION_HOUR_INDICATE_ON_10MS;
            ledState = STATE_LED_ON_HOUR;
            ledOn();
          }
        }
      }
      break;

    case STATE_LED_OFF_PAUSE:
      {
        downCounter_Led--;
        if (downCounter_Led != 0)
        {
          break;
        }

        if ( downCounter_10ms > 0 )
        {
          led_blink_count_left = getHoursFrom10msTicks_Ceil( downCounter_10ms );
          downCounter_Led = LED_DURARION_HOUR_INDICATE_ON_10MS;
          ledState = STATE_LED_ON_HOUR;
          ledOn();
        }
      }
      break;

    default:
      break;
  }

}

void task_delayer(void)
{
  debounce();

  if (button_down)
  {
    button_down = 0;
    addOneHourAndReset();

    // give instant feedback vie status LED
    downCounter_Led = 1;
    ledState = STATE_LED_OFF_PAUSE;
  }

  switch (state)
  {
    case STATE_0_WAITING_INITIAL_PRESS:
      {
        if (downCounter_10ms != 0)
        {
          state = STATE_1_COUNTING_DOWN;
        }
      }
      break;

    case STATE_1_COUNTING_DOWN:
      {
        downCounter_10ms--;
        if (downCounter_10ms == 0)
        {
          relayOn();
          state = STATE_2_RELAY_ENERGIZED;
        }
        else
        {
          ledBlink_HoursLeft();
        }
      }
      break;

    case STATE_2_RELAY_ENERGIZED:
      {
        downCounter_Relay--;
        if (downCounter_Relay == 0)
        {
          relayOff();
          state = STATE_3_RELAY_FINISHED_IDLE;
        }
      }
      break;

    case STATE_3_RELAY_FINISHED_IDLE:
      {
        ledBlink_Finished();
      }
      break;

    default:
      break;
  }
}

void setup() {
  cli(); //Disable interrupt

  flag_10ms = 0; // updated in IRQ every 10ms
  button_down = 0;
  led_blink_count_left = 0; // 0 .. 24 hours can be set
  downCounter_10ms = 0; // Hours left, quantized in 10 ms
  downCounter_Relay = RELAY_CNT;

  pinMode(LED_BUILTIN, OUTPUT); // set LED pin to OUTPUT
  pinMode(PIN_LED_EXT, OUTPUT); // set LED pin to OUTPUT
  pinMode(PIN_RELAY, OUTPUT); // relay
  pinMode(PIN_PUSHBTN, INPUT);           // set pin to input
  digitalWrite(PIN_PUSHBTN, HIGH);       // turn on pullup resistors

  // TIMER 1 for interrupt frequency 100 Hz:
  TCCR1A = 0; // set entire TCCR1A register to 0
  TCCR1B = 0; // same for TCCR1B
  TCNT1  = 0; // initialize counter value to 0
  // set compare match register for 100 Hz increments
  OCR1A = 9986; // = 8000000 / (8 * 100) - 1 (must be <65536) // Tune this value to get less or equal to 10ms interrupts at TIMER1_COMPA_vect isr
  // turn on CTC mode
  TCCR1B |= (1 << WGM12);
  // Set CS12, CS11 and CS10 bits for 8 prescaler
  TCCR1B |= (0 << CS12) | (1 << CS11) | (0 << CS10);
  // enable timer compare interrupt
  TIMSK1 |= (1 << OCIE1A);
  sei(); // allow interrupts
}



void loop() {
  // IRQ every 10ms, set on 10ms timer IRQ. Do not run if IRQ not happened
  if (flag_10ms == 1)
  {
    flag_10ms = 0;
    task_delayer();
  }
}

ISR(TIMER1_COMPA_vect) {
  //cli(); //Disable interrupt
  flag_10ms = 1;
  //sei();
}
