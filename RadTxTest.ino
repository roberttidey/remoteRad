//Main App for supporting RemoteRad operation from a web site.
//R.J.Tidey & E.K.Tidey 2016


//Use this define to include temperature measuring support
#define DS18B20 1

#include "RadTx.h"
#include "SparkIntervalTimer/SparkIntervalTimer.h"

#if defined(DS18B20)
int ONEWIRE_PIN = D5;
// This #include statement was automatically added by the Particle IDE.
#include "OneWire/OneWire.h"
// This #include statement was automatically added by the Particle IDE.
#include "spark-dallas-temperature/spark-dallas-temperature.h"
DallasTemperature dallas(new OneWire(ONEWIRE_PIN));
#endif

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

//Flash config location
#define CONFIG_FLASH 85
#define MAGIC_FLASH 165

// schedule holds 4 on/off times per day
// Each time is minutes from midnight 0-1439 11 sig bits
// If 12th bit set then time event is ensabled
int16_t schedule[8][7];
int LED = D7;
int IRLED = D0;
long timeout = 0;
int lastEventDay = 0;
int lastEventTime = -1;
int tZone = 0;
int dstType = 0;
int holidayMode = 0;
int period = 13;
int codeRepeat = 10;

//0 = Off, 1 = On
int radState = 0;
int execTimer = 0;
int execTime = 5;
int checkTime = 0;
int currentTime = 0;
float celsius = 0.0;

String strSchedule;
String strStatus;
String strConfig;
String strDebug;

//protypes
int dstNow();
int sendMsg(String command);
int sendCmd(int msgIndex);
void execSchedule();
void loadSchedule();
void saveSchedule();
void loadConfig();
void saveConfig();
int receiveSchedule(String schedule);
int receiveConfig(String strConfig);
int setNtp(String ntpSetup);
void makeStrSchedule();
void makeStrStatus();
void makeStrConfig();


void setup() {
  //Transmit on pin IRLED, 10 repeats,no invert, 100uSec tick)
#if defined(DS18B20)
    dallas.begin();
#endif
  pinMode(LED, OUTPUT);
  radtx_setup(IRLED, codeRepeat, 0, period);
  loadSchedule();
  loadConfig();
  Time.zone(tZone);
  Time.setFormat("%Y-%m-%dT%H:%M:%S");
  execTimer = Time.now();
  makeStrStatus();
  Particle.function("sendMsg", sendMsg);
  Particle.function("receiveSch", receiveSchedule);
  Particle.function("receiveConf", receiveConfig);
  Particle.variable("schedule", strSchedule);
  Particle.variable("status", strStatus);
  Particle.variable("config", strConfig);
}

void loop() {
    delay(100);
    currentTime = dstNow();
    checkTime = Time.now();
    if ((checkTime - execTimer) > execTime) {
      execTimer = checkTime;
      execSchedule();
#if defined(DS18B20)
      dallas.requestTemperatures();
      celsius = dallas.getTempCByIndex(0);
#endif
      makeStrStatus();
      makeStrConfig();
    }
}

/*
Get time adjusted by dstType
0 = Europe (Last Sunday in March Oct)
1 = USA (2nd Sunday in March, 1st Sunday in November
2 = None
Calculations good till 2099
*/

int dstNow() {
   int unixTime = Time.now();
   int day = Time.day(unixTime);
   int month = Time.month(unixTime);
   int mins = Time.hour(unixTime) * 60 + Time.minute();
   int yAdj = (Time.year(unixTime)* 5) / 4;
   int dstDayS, dstDayE;
   int dstOffset = 0;
   
   switch(dstType) {
      case 0:
         dstDayS = 31 - (4 + yAdj) % 7;
         dstDayE = 31 - (1 + yAdj) % 7; 
         if ((month > 3 && month < 10) ||
            (month == 3  && day > dstDayS) ||
            (month == 3  && day == dstDayS &&  mins > 60) ||
            (month == 10 && day < dstDayE) ||
            (month == 10  && day == dstDayE && mins < 61))
               dstOffset = 3600;
         break;
      case 1:
         dstDayS = 14 - (1 + yAdj) % 7;
         dstDayE = 7 - (1 + yAdj) % 7; 
         if ((month > 3 && month < 11) ||
            (month == 3  && day > dstDayS) ||
            (month == 3  && day == dstDayS &&  mins > 60) ||
            (month == 11 && day < dstDayE) ||
            (month == 11  && day == dstDayE && mins < 61))
               dstOffset = 3600;
         break;
   }
   return unixTime + dstOffset;
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
Particle external function to receive config values
*/
int receiveConfig(String strConfig)
{
    int index1 = 0;
    int index2 = 0;
    int configs[10];
    int config;
   
   //Parse csv into integer array
    while(config < 10 && index2 >= 0)
    {
        index2 = strConfig.indexOf(',', index1);
        if (index2 > 0)
        {
            configs[config] = strConfig.substring(index1, index2).toInt();
            index1 = index2 + 1;
        }
        else
        {
            configs[config] = strConfig.substring(index1).toInt();
        }
        config++;
    }
    //Process config values
    holidayMode = configs[0];
    dstType = configs[1];
    tZone = configs[2];
    if (period != configs[3] || codeRepeat != configs[4]) {
        period = configs[3];
        codeRepeat = configs[4];
        radtx_update(codeRepeat, period);
    }
    Time.zone(tZone);
    saveConfig();
    
    return 0;
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
    int newState;
    
    nowTime = dstNow();
    nowDay = Time.weekday(nowTime) - 1;
    nowMinute =(uint16_t)(60 * Time.hour(nowTime) + Time.minute(nowTime));
    
    //Find first Event index in the schedule for today
    for(intEventTime = 0; intEventTime < 8; intEventTime++)
    {
        eventMinute = schedule[intEventTime][nowDay];
        if(nowMinute < (eventMinute & 0x7ff))
        {
            bFound = true;
            break;
        }
    }
    if (!bFound)
    {
        intEventTime = 8;
        eventMinute = 0;
        newState = 0;
    } else
    {
        newState = intEventTime & 0x1;
    }
    lastEventTime = intEventTime;
    lastEventDay = nowDay;
    
    // In holiday always set off
    if (holidayMode != 0)
    {
        if(radState != 0)
        {
            sendCmd(0);
            radState = 0;
        }
    } else if (newState != radState)
    {
            //Only process if enable flag is clear
        if((eventMinute & 0x800) == 0)
        {
            if(newState == 0)
            {
                //Off period starting
                sendCmd(0);
                radState = 0;
            }
            else
            {
                //On period starting
                sendCmd(6);
                radState = 1;
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
function to load config from EEPROM
*/
void loadConfig()
{
    int eIndex = CONFIG_FLASH;
    
    if(EEPROM.read(eIndex++) == MAGIC_FLASH) {
        tZone = EEPROM.read(eIndex++);
        dstType = EEPROM.read(eIndex++);
        holidayMode = EEPROM.read(eIndex++);
        period = EEPROM.read(eIndex++);
        codeRepeat = EEPROM.read(eIndex++);
    }
} 

/*
function to load config from EEPROM
*/
void saveConfig()
{
    int eIndex = CONFIG_FLASH;
    
    EEPROM.write(eIndex++, MAGIC_FLASH);
    EEPROM.write(eIndex++, (uint8_t)tZone);
    EEPROM.write(eIndex++, (uint8_t)dstType);
    EEPROM.write(eIndex++, (uint8_t)holidayMode);
    EEPROM.write(eIndex++, (uint8_t)period);
    EEPROM.write(eIndex++, (uint8_t)codeRepeat);
} 

/*
function to create string version of schedule
56 values csv separated
*/
void makeStrSchedule()
{
    int intDay, intTime; 
    strSchedule = "";
    for(intDay = 0; intDay < 7; intDay++)
    {
        for(intTime = 0; intTime < 8; intTime++)
        {
           strSchedule += String(schedule[intTime][intDay]) + String(',');
        }
    }
}

/*
function to create string version of status
 values csv separated
*/
void makeStrStatus()
{
    strStatus = String(radState) + String(',') + String(lastEventDay) + String(',') + String(lastEventTime);
    strStatus += String(',') + Time.format(currentTime) + String(',') + String(celsius); + String(',') + String(strDebug);
}

/*
function to create string version of config
 values csv separated
*/
void makeStrConfig()
{
    strConfig = String(holidayMode) + String(',') + String(dstType)+ String(',') + String(tZone);
    strConfig += String(',') + String(period) + String(',') + String(codeRepeat);
}

