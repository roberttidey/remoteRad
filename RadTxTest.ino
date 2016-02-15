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
uint16_t msg[MSG_LEN * MAX_MSGS] = {BTN_ONOFF,0xffff,0,0,0,0,0,0,0,0,
                                    BTN_TEMP,0xffff,0,0,0,0,0,0,0,0,
                                    BTN_MODE,0xffff,0,0,0,0,0,0,0,0,
                                    BTN_TIME,0xffff,0,0,0,0,0,0,0,0,
                                    BTN_DOWN,0xffff,0,0,0,0,0,0,0,0,
                                    BTN_UP,0xffff,0,0,0,0,0,0,0,0,
                                    BTN_ONOFF,BTN_MODE,BTN_MODE,BTN_MODE,0xffff,0,0,0,0,0,
                                    BTN_ONOFF,BTN_MODE,BTN_MODE,BTN_MODE,0xffff,0,0,0,0,0,
                                    BTN_ONOFF,BTN_MODE,BTN_MODE,BTN_MODE,0xffff,0,0,0,0,0,
                                    0x8079,0xffff,0,0,0,0,0,0,0,0
};

// schedule holds 4 on/off times per day
// Each time is minutes from midnight 0-1439 11 sig bits
// If 12th bit set then time event is ensabled
int16_t schedule[8][7];

//protypes
int sendMsg(String command);
int sendCmd(int msgIndex);
void execSchedule();
void loadSchedule();
void saveSchedule();
int receiveSchedule(String schedule);
void makeStrSchedule();

int LED = D7;
int IRLED = D0;
long timeout = 0;
int lastEventDay = 0;
int lastEventTime = -1;
//0 = Off, 1 = On
int radState = 0;

String strCurrentTime;
String strSchedule;

void setup() {
  //Transmit on pin IRLED, 10 repeats,no invert, 100uSec tick)
  pinMode(LED, OUTPUT);
  radtx_setup(IRLED, 10, 0, 100);
  loadSchedule();
  Particle.function("sendMsg", sendMsg);
  Particle.function("receiveSch", receiveSchedule);
  Particle.variable("schedule", strSchedule);
  Particle.variable("cTime", strCurrentTime);
}

void loop() {
    delay(5000);
    strCurrentTime = Time.timeStr();
    execSchedule();
}

/*
Particle external function to Send an indexed message sequence
*/
int sendMsg(String command)
{
  int msgIndex;
  
  if(command.substring(1,1) == "$") {
      msgIndex =  MAX_MSGS - 1;
      msg[msgIndex * MSG_LEN] = (uint16_t)command.substring(2).toInt();
  } else {
       msgIndex = command.toInt();
  }
  return sendCmd(msgIndex);
  
}

/*
function to Send a message
*/
int sendCmd(int msgIndex)
{

  // look for the matching command
  if(msgIndex >= 0 && msgIndex < MAX_MSGS)
  {
    timeout = millis();
    while(!radtx_free() && millis() < (timeout + 1000))
    {
      delay(10);
    }
    digitalWrite(LED, HIGH);
    radtx_send(&msg[msgIndex * MSG_LEN]);
    timeout = millis();
    while(!radtx_free() && millis() < (timeout + 10000))
    {
      delay(10);
    }
    digitalWrite(LED, LOW);
    return radtx_debug();
  }
  else return -1;
}


/*
function to check schedule and execute commands as required
*/
void execSchedule()
{
    int nowDay, nowTime;
    uint16_t nowMinute;
    int intEventTime;
    uint16_t eventMinute;
    boolean bFound = false;
    
    nowTime = Time.now();
    nowDay = Time.day(nowTime) - 1;
    nowMinute =(uint16_t)(24 * Time.hour(nowTime) + Time.minute(nowTime));
    
    if(nowDay != lastEventDay)
    {
        // Request time synchronization from the Particle Cloud
        Particle.syncTime();
    }
    
    //Find first Event index in the schedule for today
    for(intEventTime = 0; intEventTime < 8; intEventTime++)
    {
        eventMinute = schedule[nowDay][intEventTime];
        if(nowMinute >= (eventMinute & 0x7ff))
        {
            bFound = true;
            break;
        }
    }
    //Ignore first ever events if they are off events
    if (bFound && (lastEventTime != -1 || (intEventTime & 0x1)))
    {
        //Check to see if it has been handled already
        if(intEventTime != lastEventTime || nowDay != lastEventDay)
        {
            lastEventTime = intEventTime;
            lastEventDay = nowDay;
            makeStrSchedule();
            //Only process if enable flag set
            if((eventMinute & 0x800) != 0)
            {
                if((intEventTime & 0x1) && (radState == 1))
                {
                    //Off period starting
                    sendCmd(0);
                    radState = 0;
                }
                else if(radState == 0)
                {
                    //On period starting
                    sendCmd(6);
                    radState = 1;
                }
            }
              
        }
    }
}

/*
Particle external function to receive a schedue update
schedule String is 9 numbers in a csv day,4 pairs of on/off times in minutes
*/
int receiveSchedule(String strRxSchedule)
{
    int index1 = 0;
    int index2 = 0;
    int intTime = 0;
    int intDay = -1;
    int intVal;
   
    while(intTime < 8 && index2 >= 0)
    {
        index2 = strRxSchedule.indexOf(',', index1);
        if (index2 > 0)
        {
            intVal = strRxSchedule.substring(index1, index2).toInt();
            index1 = index2 + 1;
        }
        else
        {
            intVal = strRxSchedule.substring(index1).toInt();
        }
        if (intDay < 0)
        {
            intDay = intVal;
        }
        else
        {
           schedule[intTime++][intDay] = intVal; 
        }
    }
    
    saveSchedule();
    return 0;
}

/*
function to load schedule from EEPROM
Each time pair (24 bits is packed into 3 bytes to conserve space and make CORE compaible (100 byte limit)
*/
void loadSchedule()
{
    int intDay, intTime;
    int eIndex = 0;
    uint16_t eebyte;
    
    for(intDay = 0; intDay < 7; intDay++)
    {
        for(intTime = 0; intTime < 8; intTime +=2)
        {
            schedule[intTime][intDay] = EEPROM.read(eIndex++);
            eebyte = EEPROM.read(eIndex++);
            schedule[intTime][intDay] += (eebyte << 8) & 0xf00;
            schedule[intTime + 1][intDay] = (eebyte << 4) & 0xf00;
            schedule[intTime + 1][intDay] += EEPROM.read(eIndex++);
        }
    }
    makeStrSchedule();
}

/*
function to load schedule from EEPROM
Each time pair (24 bits is packed into 3 bytes to conserve space and make CORE compaible (100 byte limit)
*/
void saveSchedule()
{
    int intDay, intTime;
    int eIndex = 0;
    
    for(intDay = 0; intDay < 7; intDay++)
    {
        for(intTime = 0; intTime < 8; intTime +=2)
        {
            EEPROM.write(eIndex++, (uint8_t)(schedule[intTime][intDay] & 0xff));
            EEPROM.write(eIndex++, (uint8_t)((schedule[intTime][intDay]  >> 8) + ((schedule[intTime + 1][intDay] >> 4) & 0xf0)));
            EEPROM.write(eIndex++, (uint8_t)(schedule[intTime + 1][intDay]  & 0xff));
        }
    }
    makeStrSchedule();
}

/*
function to create string version of schedule
radState,Day,timeperiod, + 56 values csv separated
*/
void makeStrSchedule()
{
    int intDay, intTime;           
    strSchedule = String(radState) + String(',') + String(lastEventDay) + String(',') + String(lastEventTime);
    for(intDay = 0; intDay < 7; intDay++)
    {
        for(intTime = 0; intTime < 8; intTime++)
        {
           strSchedule += String(',') + String(schedule[intTime][intDay]);
        }
    }
}