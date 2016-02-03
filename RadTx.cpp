// RadTx.cpp
//
// Radiator Infra red remote tx interface
// 
// Author: Bob Tidey (robert@tideys.net)

#include "RadTx.h"

#ifdef TX_PIN_DEFAULT
static int tx_pin = TX_PIN_DEFAULT;
#else
static int tx_pin = 3;
#endif

static const byte tx_msglen_max = 16; // Maximum number of presses in a sequence
static const uint16_t tx_msg_end = 0xffff;
static const uint16_t tx_msg_delay = 0xf000; //Numbers above this are a delay in seconds

//Transmit mode constants and variables
static byte tx_repeats = 12; // Number of repeats of each button press sent
static byte txon = 1;
static byte txoff = 0;
static boolean tx_msg_active = false; //set true to activate message sending

static uint16_t tx_buf[tx_msglen_max]; // the message buffer
static byte tx_repeat = 0; //counter for repeat of button
static byte tx_state = 0;
static byte tx_toggle_count = 1;
static int tx_debug = 0;

// These set the pulse durations in ticks
static byte tx_PulseCounts[4] = {13,4,4,13}; // ticks for 0-On,1-On,0-Off,1-Off
static byte tx_gap1_count = 66; // Repeat press gap count (6.6 msec)
static byte tx_gap2_mult = 250; // Long Gap muliplier 25mS
static uint16_t tx_gap2_count = 20; // Inter message code gap count (units of 25mS)

static const byte tx_state_idle = 0;
static const byte tx_state_msgStart = 1;
static const byte tx_state_buttonStart = 2;
static const byte tx_state_sendBitOn = 3;
static const byte tx_state_sendBitOff = 4;
static const byte tx_state_buttonEnd = 5;
static const byte tx_state_gap1Start = 6;
static const byte tx_state_gap1End = 7;
static const byte tx_state_gap2Start = 8;
static const byte tx_state_gap2End = 9;
static const byte tx_state_delayStart = 10;
static const byte tx_state_delayEnd = 11;

static uint16_t tx_bit_mask = 0; // bit mask in current button
static byte tx_num_buttons = 0; // number of buttons sent
static uint16_t tx_delay_counter = 0; 
static uint16_t tx_delay_count = 0; 

void isrTXtimer() {
   //Set low after toggle count interrupts
   tx_toggle_count--;
   if (tx_toggle_count < 1) {
     switch (tx_state) {
       case tx_state_idle:
         if(tx_msg_active) {
           tx_repeat = 0;
           tx_toggle_count = 1;
           tx_state = tx_state_msgStart;
         }
         break;
       case tx_state_msgStart:
         digitalWrite(tx_pin, txoff);
         tx_toggle_count = 1;
         tx_num_buttons = 0;
         tx_state = tx_state_buttonStart;
         break;
       case tx_state_buttonStart:
         if(tx_buf[tx_num_buttons] <= 0xfff) {
           tx_toggle_count = 1;
           tx_bit_mask = 0x800;
           tx_state = tx_state_sendBitOn;
         } else if (tx_buf[tx_num_buttons] == 0xffff) {
           //disable timer interrupt
           rad_timer_Stop();
           tx_msg_active = false;
           tx_toggle_count = 1;
           tx_state = tx_state_idle;
         } else {
           tx_toggle_count = 1;
           tx_state = tx_state_delayStart;
         }
         break;
       case tx_state_sendBitOn:
         digitalWrite(tx_pin, txon);
         if(tx_buf[tx_num_buttons] & tx_bit_mask) {
           tx_toggle_count = tx_PulseCounts[1];
         } else {
           tx_toggle_count = tx_PulseCounts[0];
         }
         tx_state = tx_state_sendBitOff;
         break;
       case tx_state_sendBitOff:
         digitalWrite(tx_pin, txoff);
         if(tx_buf[tx_num_buttons] & tx_bit_mask) {
           tx_toggle_count = tx_PulseCounts[3];
         } else {
           tx_toggle_count = tx_PulseCounts[2];
         }
         tx_bit_mask >>=1;
         if(tx_bit_mask == 0) {
           tx_state = tx_state_gap1Start;
         } else {
            tx_state = tx_state_sendBitOn;
         }
         break;
       case tx_state_gap1Start:
         tx_toggle_count = tx_gap1_count;
         tx_state = tx_state_gap1End;
         break;
       case tx_state_gap1End:
         tx_repeat++;
         if(tx_repeat < tx_repeats) {
           tx_toggle_count = 1;
           tx_state = tx_state_buttonStart;
         } else {
           tx_state = tx_state_gap2Start;
         }
         break;
       case tx_state_gap2Start:
         tx_toggle_count = tx_gap2_mult;
         tx_delay_counter = 0;
         tx_state = tx_state_gap2End;
         break;
       case tx_state_gap2End:
         tx_delay_counter++;
         if(tx_delay_counter >= tx_gap2_count) {
           tx_toggle_count = 1;
           tx_num_buttons++;
           tx_repeat = 0;
           tx_state = tx_state_buttonStart;
         } else {
           tx_toggle_count = tx_gap2_mult;
         }
         break;
       case tx_state_delayStart:
         tx_toggle_count = tx_gap2_mult;
         tx_delay_counter = 0;
         tx_delay_count = tx_buf[tx_num_buttons] & 0x7fff;
         tx_state = tx_state_delayEnd;
         break;
       case tx_state_delayEnd:
         tx_delay_counter++;
         if(tx_delay_counter >= tx_delay_count) {
           tx_toggle_count = 1;
           tx_num_buttons++;
           tx_state = tx_state_buttonStart;
         } else {
           tx_toggle_count = tx_gap2_mult;
         }
         break;
     }
   }
}

/**
  Check for send free
**/
boolean radtx_free() {
  return !tx_msg_active;
}

/**
  Return debg int
**/
int radtx_debug() {
  return tx_debug;
}

/**
  Send a radiator message (12 bit messages) terminated by 0xffff
**/
void radtx_send(uint16_t *msg) {
  int index = 0;
  do {
    tx_buf[index] = msg[index];
    index++;
  } while (index < tx_msglen_max && msg[index - 1] != 0xffff);
  rad_timer_Start();
  tx_msg_active = true;
}

/**
  Set things up to transmit messages
**/
void radtx_setup(int pin, byte repeats, byte invert, int period) {
	if(pin >= 0 && pin <= 7) {
		tx_pin = pin;
	}
	pinMode(tx_pin,OUTPUT);
	digitalWrite(tx_pin, txoff);
	
	if(repeats > 0 && repeats < 40) {
	 tx_repeats = repeats;
	}
	if(invert != 0) {
	 txon = 0;
	 txoff = 1;
	} else {
	 txon = 1;
	 txoff = 0;
	}
	
	int period1;
	if (period > 32 && period < 1000) {
		period1 = period; 
	} else {
		//default 140 uSec
		period1 = 140;
	}
	rad_timer_Setup(isrTXtimer, period1);
}

// There are 3 timer support routines. Variants of these may be placed here to support different environments
void (*isrRoutine) ();
#if defined(SPARK_CORE)
//#include "SparkIntervalTimer.h"
//reference for library when imported in Spark IDE
#include "SparkIntervalTimer/SparkIntervalTimer.h"
IntervalTimer txmtTimer;

extern void rad_timer_Setup(void (*isrCallback)(), int period) {
	isrRoutine = isrCallback;
	noInterrupts();
	txmtTimer.begin(isrRoutine, period, uSec);	//set IntervalTimer interrupt at period uSec (default 140)
	txmtTimer.interrupt_SIT(INT_DISABLE); // initialised as off, first message starts it
	interrupts();
}
extern void rad_timer_Start() {
	txmtTimer.interrupt_SIT(INT_ENABLE);
}
extern void rad_timer_Stop() {
	txmtTimer.interrupt_SIT(INT_DISABLE);
}

#elif defined(DUE)
#include "DueTimer.h"
DueTimer txmtTimer = DueTimer::DueTimer(0);
boolean dueDefined = false;
extern void rad_timer_Setup(void (*isrCallback)(), int period) {
	if (!dueDefined) {
		txmtTimer = DueTimer::getAvailable();
		dueDefined = true;
	}
	isrRoutine = isrCallback;
	noInterrupts();
	txmtTimer.attachInterrupt(isrCallback);
	txmtTimer.setPeriod(period);
	interrupts();
}
extern void rad_timer_Start() {
	txmtTimer.start();
}

extern void rad_timer_Stop() {
	txmtTimer.stop();
}

#elif defined(PRO32U4)
//32u4 which uses the TIMER3
extern void rad_timer_Setup(void (*isrCallback)(), int period) {
    isrRoutine = isrCallback; // unused here as callback is direct
    byte clock = (period / 4) - 1;;
    cli();//stop interrupts
    //set timer2 interrupt at  clock uSec (default 140)
    TCCR3A = 0;// set entire TCCR2A register to 0
    TCCR3B = 0;// same for TCCR2B
    TCNT3  = 0;//initialize counter value to 0
    // set compare match register for clock uSec
    OCR3A = clock;// = 16MHz Prescale to 4 uSec * (counter+1)
    // turn on CTC mode
    TCCR3B |= (1 << WGM32);
    // Set bits for 64 prescaler
    TCCR3B |= (1 << CS31);   
    TCCR3B |= (1 << CS30);   
    // disable timer compare interrupt
    TIMSK3 &= ~(1 << OCIE3A);
    sei();//enable interrupts
}

extern void rad_timer_Start() {
   //enable timer 2 interrupts
    TIMSK3 |= (1 << OCIE3A);
}

extern void rad_timer_Stop() {
    //disable timer 2 interrupt
    TIMSK3 &= ~(1 << OCIE3A);
}

//Hand over to real isr
ISR(TIMER3_COMPA_vect){
    isrTXtimer();
}

#else
//Default case is Arduino Mega328 which uses the TIMER2
extern void rad_timer_Setup(void (*isrCallback)(), int period) {
	isrRoutine = isrCallback; // unused here as callback is direct
	byte clock = (period / 4) - 1;;
	cli();//stop interrupts
	//set timer2 interrupt at  clock uSec (default 140)
	TCCR2A = 0;// set entire TCCR2A register to 0
	TCCR2B = 0;// same for TCCR2B
	TCNT2  = 0;//initialize counter value to 0
	// set compare match register for clock uSec
	OCR2A = clock;// = 16MHz Prescale to 4 uSec * (counter+1)
	// turn on CTC mode
	TCCR2A |= (1 << WGM21);
	// Set CS11 bit for 64 prescaler
	TCCR2B |= (1 << CS22);   
	// disable timer compare interrupt
	TIMSK2 &= ~(1 << OCIE2A);
	sei();//enable interrupts
}

extern void rad_timer_Start() {
   //enable timer 2 interrupts
	TIMSK2 |= (1 << OCIE2A);
}

extern void rad_timer_Stop() {
	//disable timer 2 interrupt
	TIMSK2 &= ~(1 << OCIE2A);
}

//Hand over to real isr
ISR(TIMER2_COMPA_vect){
	isrTXtimer();
}

#endif

