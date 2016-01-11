#include <RadTx.h>
//on,mode,mode,mode,delay1200,off,delay1200,end
uint16_t msg[] = {0x027e,0x026f,0x026f,0x026f,0x84b0,0x027e,0x84b0,0xffff};
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

