// This #include statement was automatically added by the Particle IDE.
#include "RadTx.h"

// This #include statement was automatically added by the Particle IDE.
#include "SparkIntervalTimer/SparkIntervalTimer.h"

#define MAX_MSGS 10
#define MSG_LEN 10
// msg0 on-off,end
// msg1 temp,end
// msg2 mode,end
// msg3 time,end
// msg4 down,end
// msg5 up,end
// msg6 on,mode,mode,mode,delay1000,off,delay1000,end
// msg7 on,mode,mode,mode,delay1000,off,delay1000,end
// msg8 on,mode,mode,mode,delay1000,off,delay1000,end
// msg9 delay3000,end
uint16_t msg[MSG_LEN * MAX_MSGS] = {0x027e,0xffff,0,0,0,0,0,0,0,0,
                                    0x0277,0xffff,0,0,0,0,0,0,0,0,
                                    0x026f,0xffff,0,0,0,0,0,0,0,0,
                                    0x0277,0xffff,0,0,0,0,0,0,0,0,
                                    0x0239,0xffff,0,0,0,0,0,0,0,0,
                                    0x027d,0xffff,0,0,0,0,0,0,0,0,
                                    0x027e,0x026f,0x026f,0x026f,0x8028,0x027e,0x8028,0xffff,0,0,
                                    0x027e,0x026f,0x026f,0x026f,0x8028,0x027e,0x8028,0xffff,0,0,
                                    0x027e,0x026f,0x026f,0x026f,0x8028,0x027e,0x8028,0xffff,0,0,
                                    0x8078,0xffff,0,0,0,0,0,0,0,0
};
long timeout = 0;
int sendMsg(String command);
int LED = D7;
int IRLED = D0;

void setup() {
  //Transmit on pin IRLED, 10 repeats,no invert, 100uSec tick)
  pinMode(LED, OUTPUT);
  radtx_setup(IRLED, 10, 0, 100);
  Particle.function("sendMsg", sendMsg);
}

void loop() {
}

int sendMsg(String command)
{
  int msgIndex = atoi(command);
  
  // look for the matching command
  if(msgIndex >= 0 && msgIndex < MAX_MSGS)
  {
    timeout = millis();
    while(!radtx_free() && millis() < (timeout + 1000)) {
      delay(10);
    }
    digitalWrite(LED, HIGH);
    radtx_send(&msg[msgIndex * MSG_LEN]);
    timeout = millis();
    while(!radtx_free() && millis() < (timeout + 10000)) {
      delay(10);
    }
    digitalWrite(LED, LOW);
    return tx_debug();
  }
  else return -1;
}