/* Fermentation Temp Logger 
   Author: Matt Boykin
   Date: 3-25-2014
   File: FermentationTempLogger.ino
   
   This sketch is used to track and log fermater temperature and room temperature and humidity.
   
*/
#include <dht.h>
#include <SD.h>
#include <Wire.h>
#include <SPI.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "RTClib.h"

#define dht_dpin A0 

dht DHT;

// how many milliseconds between grabbing data and logging it. 1000 ms is once a second
#define LOG_INTERVAL  30000 // mills between entries (reduce to take more/faster data)

// how many milliseconds before writing the logged data permanently to disk
#define SYNC_INTERVAL 30000 // mills between calls to flush() - to write data to the card
uint32_t syncTime = 0; // time of last sync()

#define ECHO_TO_SERIAL   1 // echo data to serial port

#define DHT11TEMPC_CAL -0.5
#define DHT11HUMD_CAL 10

#define ONE_WIRE_BUS 3
OneWire tempWire(ONE_WIRE_BUS);
DallasTemperature sensors(&tempWire);

//Define addresses of temp sensors
DeviceAddress wortThermo = { 0x28, 0xCE, 0x8A, 0x26, 0x06, 0x00, 0x00, 0x4C };

RTC_DS1307 RTC; // define the Real Time Clock object

// for the data logging shield, we use digital pin 10 for the SD cs line
const int chipSelect = 10;

// the logging file
File logfile;
char filename[] = "00000000.CSV";
DateTime lastFileCreation;

void error(char *str)
{
  Serial.print("error: ");
  Serial.println(str);

  while(1);
}

//Celsius to Fahrenheit conversion
double Fahrenheit(double celsius)
{
	return 1.8 * celsius + 32;
}

void getFileName(){
  lastFileCreation = RTC.now();
  filename[0] = (lastFileCreation.year()/1000)%10 + '0'; //To get 1st digit from year()
  filename[1] = (lastFileCreation.year()/100)%10 + '0'; //To get 2nd digit from year()
  filename[2] = (lastFileCreation.year()/10)%10 + '0'; //To get 3rd digit from year()
  filename[3] = lastFileCreation.year()%10 + '0'; //To get 4th digit from year()
  filename[4] = lastFileCreation.month()/10 + '0'; //To get 1st digit from month()
  filename[5] = lastFileCreation.month()%10 + '0'; //To get 2nd digit from month()
  filename[6] = lastFileCreation.day()/10 + '0'; //To get 1st digit from day()
  filename[7] = lastFileCreation.day()%10 + '0'; //To get 2nd digit from day()
  Serial.println(filename);
}

void createFileName(){
    Serial.println(filename);
    logfile = SD.open(filename, FILE_WRITE);
}

void setup(){
  Serial.begin(9600);
  
  //Start DS18b20
  sensors.begin();
  sensors.setResolution(wortThermo,10);
  
  // initialize the SD card
  Serial.print("Initializing SD card...");
  // make sure that the default chip select pin is set to
  // output, even if you don't use it:
  pinMode(10, OUTPUT);
  
  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
    error("Card failed, or not present");
  }
  Serial.println("card initialized.");
  
  // connect to RTC
  Wire.begin();  
  if (!RTC.begin()) {
    Serial.println("RTC failed");
  }
  
  getFileName();
  createFileName();
  
  if (! logfile) {
    error("couldnt create file");
  }
  
  Serial.print("Logging to: ");
  Serial.println(filename);

  
  
  //Sets the time when compiled only uncomment when real time clock dies
  //RTC.adjust(DateTime(__DATE__, __TIME__));
    
#if ECHO_TO_SERIAL
  Serial.println("Letting system settle to begin taking readings.");
#endif //ECHO_TO_SERIAL

  delay(2000);//Let system settle
  
  logfile.println("millis,stamp,datetime,humidity,temp C,temp F,wort temp C,wort temp F,diff C");    
#if ECHO_TO_SERIAL
  Serial.println("millis,stamp,datetime,humidity,temp C,temp F,wort temp C,wort temp F,diff C");
#endif //ECHO_TO_SERIAL
  
}// end setup()

void loop(){
  
  DateTime now;
  DHT.read11(dht_dpin);
  sensors.requestTemperatures();
  float wortTempC = sensors.getTempC(wortThermo);
  
  // delay for the amount of time we want between readings
  delay((LOG_INTERVAL -1) - (millis() % LOG_INTERVAL));
  
  // log milliseconds since starting
  uint32_t m = millis();
  logfile.print(m);           // milliseconds since start
  logfile.print(", ");    
#if ECHO_TO_SERIAL
  Serial.print(m);         // milliseconds since start
  Serial.print(", ");  
#endif

  // fetch the time
  now = RTC.now();
  // log time
  logfile.print(now.unixtime()); // seconds since 1/1/1970
  logfile.print(", ");
  logfile.print('"');
  logfile.print(now.year(), DEC);
  logfile.print("/");
  logfile.print(now.month(), DEC);
  logfile.print("/");
  logfile.print(now.day(), DEC);
  logfile.print(" ");
  logfile.print(now.hour(), DEC);
  logfile.print(":");
  logfile.print(now.minute(), DEC);
  logfile.print(":");
  logfile.print(now.second(), DEC);
  logfile.print('"');
#if ECHO_TO_SERIAL
  Serial.print(now.unixtime()); // seconds since 1/1/1970
  Serial.print(", ");
  Serial.print('"');
  Serial.print(now.year(), DEC);
  Serial.print("/");
  Serial.print(now.month(), DEC);
  Serial.print("/");
  Serial.print(now.day(), DEC);
  Serial.print(" ");
  Serial.print(now.hour(), DEC);
  Serial.print(":");
  Serial.print(now.minute(), DEC);
  Serial.print(":");
  Serial.print(now.second(), DEC);
  Serial.print('"');
#endif //ECHO_TO_SERIAL
  
  logfile.print(",");    
  logfile.print(DHT.humidity + DHT11HUMD_CAL);
  logfile.print("% ");
  logfile.print(",");
  logfile.print(DHT.temperature + DHT11TEMPC_CAL); 
  logfile.print(",");
  logfile.print(Fahrenheit(DHT.temperature + DHT11TEMPC_CAL));
  logfile.print(",");
  logfile.print(wortTempC); 
  logfile.print(",");
  logfile.print(Fahrenheit(wortTempC));
  logfile.print(","); 
  logfile.print(wortTempC-DHT.temperature);
#if ECHO_TO_SERIAL
  Serial.print(",");    
  Serial.print(DHT.humidity + DHT11HUMD_CAL);
  Serial.print("% ");
  Serial.print(",");
  Serial.print(DHT.temperature + DHT11TEMPC_CAL); 
  Serial.print(",");
  Serial.print(Fahrenheit(DHT.temperature + DHT11TEMPC_CAL));
  Serial.print(",");
  Serial.print(wortTempC); 
  Serial.print(",");
  Serial.print(Fahrenheit(wortTempC));
  Serial.print(","); 
  Serial.print(wortTempC-(DHT.temperature + DHT11TEMPC_CAL));
#endif //ECHO_TO_SERIAL

  logfile.println();
#if ECHO_TO_SERIAL
  Serial.println();
#endif // ECHO_TO_SERIAL

  // Now we write data to disk! Don't sync too often - requires 2048 bytes of I/O to SD card
  // which uses a bunch of power and takes time
  if ((millis() - syncTime) < SYNC_INTERVAL) return;
  syncTime = millis();
  
  logfile.flush();
  
  if (lastFileCreation.day() != now.day())
  {
    logfile.close();
    getFileName();
    createFileName();
  }
  
  
  
}// end loop()
