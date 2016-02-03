// RadTx.h
//
// IR tx interface for Radiator remote control
// 
// Author: Bob Tidey (robert@tideys.net)
//Choose one of the following environments to compile for. Only one should be defined
//For SparkCore the SparkIntervalTimer Library code needs to be present
//For Due the DueTimer library code needs to be present

//#define SPARK_CORE 1
//#define DUE 1
//#define PRO32U4 1
//#define AVR328 1
#define SPARK_CORE 1


//Include basic library header and set default TX pin
#ifdef SPARK_CORE
#include "application.h"
#define TX_PIN_DEFAULT D3
#elif DUE
#include <Arduino.h>
#define TX_PIN_DEFAULT 3
#else
#include <Arduino.h>
#define TX_PIN_DEFAULT 3
#endif

//Sets up basic parameters must be called at least once
extern void radtx_setup(int pin, byte repeats, byte invert, int uSec);

//Checks whether tx is free to accept a new message
extern boolean radtx_free();

//Basic send of new message ( each button press is 12 bits in a word)
extern void radtx_send(uint16_t* msg);

//Genralised timer routines go here
//Sets up timer and the callback to the interrupt service routine
void rad_timer_Setup(void (*isrCallback)(), int period);

void rad_timer_Start();

void rad_timer_Stop();
