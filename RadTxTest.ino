#include <RadTx.h>

uint16_t msg[] = {4,0,3,0,5,9,3,0,1,2};
long timeout = 0;

void setup() {
  //Transmit on pin 7, 10 repeats,no invert, 100uSec tick)
  radtx_setup(7, 10, 0, 140);
}

void loop() {
  if (radtx_free()) {
    radtx_send(msg);
    timeout = millis();
  }
  while(!radtx_free() && millis() < (timeout + 1000)) {
    delay(10);
  }
  timeout = millis() - timeout;
  delay(10000);
}

