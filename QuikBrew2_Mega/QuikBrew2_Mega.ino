/* QuikBrew V2.0 MEGA
   Author: Matt Boykin
   Date: 7-5-2015
   File: QuikBrew2_Mega.ino
   
   This is a semi-automated home brewery with
   1 transfer pump 
   1 immersion chiller pump
   4 thermocouples
   1 LCD Display 4x20
   2 Buttons
   3 LEDs
   SD Card Recipe Loading
   
   States and Phases:
   0 - Powerup
   1 - Idle
   2 - Sanitize
   3 - Mash
     1 - Heat water
     2 - Hot water transfer
     3 - Mash (timer)
     4 - Vorlauf
     5 - HLTHeat
     6 - Sparge/Transfer   
   4 - Boil
     1 - Heat water
     2 - Boil Wort
     2 - Cool Wort
     3 - Wort Transfer
     
     
  RECIPE Example:
    HLTMASHTEMP=70
    HLTSPRGTEMP=70
    MASHLENGTHSEC=90
    SPRGSTARTHEATSEC=20
    HLTBOILTEMP=70
    BOILLENGTHSEC=60
    COOLWORTTEMP=70
    HOPADD1=60
    HOPADD2=30
    HOPADD3=10
    HOPADD4=0
    HOPADD5=-1
    HOPADD6=-1
    HOPADD7=-1
    HOPADD8=-1
    HOPADD9=-1
    HOPADD10=-1
 
 */
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <LiquidCrystal_I2C.h>
#include <SD.h>
#include <SPI.h>

/* LCD Setup */
LiquidCrystal_I2C lcd(0x3f,20,4);

/* DO */
int transferPump = 28;
int chillerPump = 38;
int readyLED = 34;
int power2Pump = 40;

/* DI */
int changeStateButton = 30;
int proceedButton = 32;

/* SD Pin Out for Mega 2560
SS	53
MOSI	51
MISO	50
SCK	52
*/
int SDPin = 53;

/* AI */
//Define wire for temperature sensor bus
#define ONE_WIRE_BUS 36
OneWire tempWire(ONE_WIRE_BUS);
DallasTemperature sensors(&tempWire);

//Define addresses of temp sensors
DeviceAddress hltThermo = { 0x28, 0x4F, 0xF4, 0x8C, 0x05, 0x00, 0x00, 0x08 };
DeviceAddress chillerThermo = { 0x28, 0x12, 0xE9, 0x26, 0x06, 0x00, 0x00, 0xAA };
DeviceAddress mashThermo = { 0x28, 0x8A, 0x7F, 0xF3, 0x04, 0x00, 0x00, 0x5E };
DeviceAddress wortThermo = { 0x28, 0x39, 0xD8, 0xF3, 0x04, 0x00, 0x00, 0x62 };

//According to temp sensor doc it takes 750ms to get temp so no sense in polling too much
int tempPollRateSec = 3;
int lastPollSec = 0;

/* Phase Timers */
int mashStartMillis = 0;
int mashTimer = 0;
int boilStartMillis = 0;
int boilTimer = 0;
int coolStartMillis = 0;
int timeToCool = 0;

/* Global Variables */
int state = 1;
int phase = 0;
float wortTemp = 0;
float hltTemp = 0;
float mashTemp = 0;
boolean degreeF = true; //if false then use C
boolean needProceed = false;
String statePhase;
int hopIndex = 0;
int hopAddLength;
bool recipeLoaded;

/* Mash Settings - Loaded from SD Card - Defaults */
float hotWaterTempTarget = 175;
float spargeTempTarget = 180;
int mashLengthSec = 3600;
int mashStartHeatSpargeSec = 900;

/* Boil Settings - Loaded from SD Card - Defaults */
int boilLengthSec = 3600;
int coolTargetTemp = 80;
int boilTemp = 200;

//Hop Additions will be added from recipe on SD Card
int hopAdditions[10];
int hopAddSec = 60; //Amount of time to alert to add hops


/* ENUMS */
enum state {
  Powerup = 0,
  Idle,
  Sanitize,
  Mash,
  Boil
};

enum mashPhase {
  HeatMashWater = 1,
  HWTransfer,
  Mashing,
  Vorlauf,
  HLTHeat,
  Sparge
};

enum boilPhase {
  HeatBoilWater = 1,
  BoilWort,
  CoolWort,
  TransferWort
};


void setup(){
  
  // LCD setup 
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0,0);
  lcd.print("Quikbrew V2.1");
  lcd.setCursor(0,2);
  lcd.print("Powering up...");
  
  //Set state and phase
  state = Powerup;
  phase = 0;
  
  recipeLoaded = false;
  
  /* DO */
  pinMode(transferPump, OUTPUT);
  pinMode(chillerPump, OUTPUT);
  pinMode(readyLED, OUTPUT);
  pinMode(power2Pump, OUTPUT);
  
  /* DI */
  pinMode(changeStateButton, INPUT);
  pinMode(proceedButton, INPUT);
  
  //SD Card initiliazation
  pinMode(SDPin, OUTPUT);
  SD.begin(SDPin);
  
  /* AI */
  //Start temperature sensors
  sensors.begin();
  sensors.setResolution(wortThermo,10);
  sensors.setResolution(chillerThermo,10);
  sensors.setResolution(hltThermo,10);
  sensors.setResolution(mashThermo,10);

  //Give power to other relays
  digitalWrite(power2Pump,LOW);
  
  //Go to an idle state
  //Set state and phase
  state = Idle;
  phase = 1;
  hopAddLength = sizeof(hopAdditions)/sizeof(int);
  
  //Load Recipe from SD Card
  loadRecipe();
}

float getTemperature(DeviceAddress thermoAddress){
  float tempF;
  float tempC = sensors.getTempC(thermoAddress);
  if(tempC == -127.00){
    //error
    return -99.99;
  }else{
    //return F or C based on settings
    if (degreeF){
       tempF = DallasTemperature::toFahrenheit(tempC);
       return tempF;
    }else{
      return tempC;
    } 
  }
}

void changeCurrentState(void){
  if (state != Boil){
    state ++;
    phase = 1;
  }else{
    state = Idle;
    phase = 1;
  }
}

void clearLine(int lineNum){
  lcd.setCursor(0,lineNum);
  lcd.print("                    ");
}

void loadRecipe(void){
  File myFile;
  char ch;
  String val;
  String attr;
  bool eqlFND;
  
  if (SD.exists("recipe.txt")) {
    
    myFile = SD.open("recipe.txt");
    eqlFND = false;
    val = "";
    attr= "";
    while(myFile.available())
    {
      ch = myFile.read();
      
      if(ch=='='){
        eqlFND = true;
      }
      else if(ch=='\n')
      {
        
         if (attr == "HLTMASHTEMP")
         {
            hotWaterTempTarget = val.toInt();
         }
         else if (attr == "HLTSPRGTEMP")
         {
            spargeTempTarget = val.toInt();
         }
         else if (attr == "HLTBOILTEMP")
         {
            boilTemp = val.toInt();
         }
         else if (attr == "COOLWORTTEMP")
         {
            coolTargetTemp = val.toInt();
         }
         else if (attr == "MASHLENGTHSEC")
         {
            mashLengthSec = val.toInt();
         }
         else if (attr == "SPRGSTARTHEATSEC")
         {
            mashStartHeatSpargeSec = val.toInt();
         }   
         else if (attr == "BOILLENGTHSEC")
         {
            boilLengthSec = val.toInt();
         }      
         else if (attr == "HOPADD1")
         {
            hopAdditions[0] = val.toInt();
         }
         else if (attr == "HOPADD2")
         {
            hopAdditions[1] = val.toInt();
         }
         else if (attr == "HOPADD3")
         {
            hopAdditions[2] = val.toInt();
         }
         else if (attr == "HOPADD4")
         {
            hopAdditions[3] = val.toInt();
         }
         else if (attr == "HOPADD5")
         {
            hopAdditions[4] = val.toInt();
         }
         else if (attr == "HOPADD6")
         {
            hopAdditions[5] = val.toInt();
         }
         else if (attr == "HOPADD7")
         {
            hopAdditions[6] = val.toInt();
         }
         else if (attr == "HOPADD8")
         {
            hopAdditions[7] = val.toInt();
         }
         else if (attr == "HOPADD9")
         {
            hopAdditions[8] = val.toInt();
         }
         else if (attr == "HOPADD10")
         {
            hopAdditions[9] = val.toInt();
         }
   
        val = "";
        attr = "";
        eqlFND = false;
      }
      else
      {
        if(eqlFND)
        {
          val += ch;
        }
        else
        {
          attr += ch;
        }
      }
    }
    myFile.close();
    recipeLoaded = true;
  }
}

void loop(void){
  int tmrMin = 0;
  int tmrSec = 0;
  char timer[20];
  char tempDisplay[20];
  int currentMillis = millis()/1000;
  int proceed = digitalRead(proceedButton);
  int changeState = digitalRead(changeStateButton);
  boolean gotTemp = false;
  
  //Only get temp at poll rate
  if (currentMillis == 0 || ((currentMillis-lastPollSec) >= tempPollRateSec))
  {
      sensors.requestTemperatures();
      gotTemp = true;
      lastPollSec = currentMillis;
  }
  
  if (changeState == LOW){
     changeCurrentState();
     clearLine(1);
     clearLine(2);
     clearLine(3);
     digitalWrite(transferPump,LOW);
     digitalWrite(chillerPump,LOW);
     delay(500); //Delay in case someone is holding the button down
  }
  
  switch (state){
    case Idle:
      //Print some text to LCD here
      if (statePhase != "Idle"){
        clearLine(0);
        clearLine(1);
        clearLine(2);
        clearLine(3);
        lcd.setCursor(0,0);
        lcd.print("Quikbrew V2.0 MEGA");
        statePhase = "Idle";
        
        if (recipeLoaded)
        {
          lcd.setCursor(0,1);
          lcd.print("Recipe Loaded");
        }
        
        lcd.setCursor(0,2);
        lcd.print("Ready to brew....");
        lcd.setCursor(0,3);
        lcd.print("Press 1 to Start");
      }
      
      //Set to idle state
      needProceed = false;
      digitalWrite(readyLED,LOW);
      digitalWrite(transferPump,LOW);
      digitalWrite(chillerPump,LOW);
      phase = 0;
      
      break;
     
    case Sanitize:
       
       if (statePhase != "Sanitize"){
        clearLine(1);
        clearLine(2);
        clearLine(3);
        statePhase = "Sanitize";
        lcd.setCursor(0,1);
        lcd.print(statePhase);
        lcd.setCursor(0,2);
        lcd.print("Cleaning Time");
        lcd.setCursor(0,3);
        lcd.print("Press 2 to Start");
        digitalWrite(readyLED,HIGH);
        delay(500); //Delay in case someone is holding the button down
       }
       
       else if (proceed == LOW){
         clearLine(2);
         clearLine(3);
         lcd.setCursor(0,2);
         lcd.print("Cleaning");
         lcd.setCursor(0,3);
         lcd.print("Press 1 to Mash");
         
         //Delay before cutting pump on as the system needs to settle before cutting on pump due to amp pull
         delay(1000);
         digitalWrite(transferPump,HIGH);
       }
       
       break;
       
    case Mash:
     //Only update when temp requested
     if (gotTemp){
        hltTemp = getTemperature(hltThermo);
        mashTemp = getTemperature(mashThermo);
        wortTemp = 0.0;
      }
      
      //LCD Print temp hot water pot
      sprintf(tempDisplay,"HLT: %d Mash: %d ",int(hltTemp), int(mashTemp));
      
      lcd.setCursor(0,1);
      lcd.print(tempDisplay);
      
      switch (phase){
        case HeatMashWater:
          if (statePhase != "Mash - Heat Water"){
             clearLine(0);
             clearLine(2);
             clearLine(3);
             statePhase = "Mash - Heat Water";
             lcd.setCursor(0,0);
             lcd.print(statePhase);
             sprintf(tempDisplay,"TARGET: %d   ",int(hotWaterTempTarget));
             lcd.setCursor(0,2);
             lcd.print(tempDisplay);
             delay(500); //Delay in case someone is holding the button down
             
          }else if (needProceed && (proceed == LOW)){
            phase = HWTransfer;
            needProceed = false;
            digitalWrite(readyLED,LOW);
          }
          
          if (hltTemp >= (hotWaterTempTarget * 0.95)){
             digitalWrite(readyLED,HIGH);
             needProceed = true;
             lcd.setCursor(0,3);
             lcd.print("Press 2 to Transfer");
             
          }else{
             digitalWrite(readyLED,LOW);
             needProceed = false;
          }
          break;
          
        case HWTransfer:
          
          //LCD Press proceed to begin Mash timer
          if (statePhase != "Mash - H2O Transfer"){
             clearLine(0);
             clearLine(2);
             statePhase = "Mash - H2O Transfer";
             lcd.setCursor(0,0);
             lcd.print(statePhase);
             clearLine(3);
             lcd.setCursor(0,3);
             lcd.print("Press 2 -Start Timer");
             digitalWrite(readyLED,LOW);
             delay(1000); //Delay before cutting pump on as the system needs to settle before cutting on pump due to amp pull
             digitalWrite(transferPump,HIGH);
             
          }else if (proceed == LOW){
            phase = Mashing;
            mashStartMillis = millis()/1000;
            needProceed = false;
            digitalWrite(transferPump,LOW);
          }
          break;
          
        case Mashing:
          mashTimer = currentMillis - mashStartMillis;
          mashTimer = mashLengthSec - mashTimer;
          tmrMin = mashTimer / 60;
          tmrSec = mashTimer % 60;
          sprintf(timer,"Time Left %02d:%02d     ",tmrMin,tmrSec);
          
          if (statePhase != "Mash - Mashing"){
             clearLine(0);
             clearLine(2);
             clearLine(3);
             statePhase = "Mash - Mashing";
             lcd.setCursor(0,0);
             lcd.print(statePhase);
             delay(500); //Delay in case someone is holding the button down
             
          }else if (needProceed && (proceed == LOW)){
            phase = Vorlauf;
            digitalWrite(readyLED,LOW);
            needProceed = false;
            mashStartMillis = millis()/1000;
          }
          
          if (mashTimer <= 0){
            //LCD mash done press proceed to start sparge pump
            digitalWrite(readyLED,HIGH);
            needProceed = true;
            lcd.setCursor(0,2);
            lcd.print(timer);
            lcd.setCursor(0,3);
            lcd.print("Press 2 to Vorlauf");
            
          }else if (mashTimer <= mashStartHeatSpargeSec){
            digitalWrite(readyLED,HIGH);
            lcd.setCursor(0,2);
            lcd.print(timer);
            
          }else{
            digitalWrite(readyLED,LOW);
            lcd.setCursor(0,2);
            lcd.print(timer);
          }
          break;
          
        case Vorlauf:
          mashTimer = currentMillis - mashStartMillis;
          tmrMin = mashTimer / 60;
          tmrSec = mashTimer % 60;
          sprintf(timer,"Time %02d:%02d     ",tmrMin,tmrSec);
          
          if (statePhase != "Mash - Vorlauf"){
            
             clearLine(0);
             clearLine(2);
             clearLine(3);
             statePhase = "Mash - Vorlauf";
             lcd.setCursor(0,0);
             lcd.print(statePhase);
             lcd.setCursor(0,3);
             lcd.print("Press 2 to Sparge");
             delay(1000); //Delay before cutting pump on as the system needs to settle before cutting on pump due to amp pull
             digitalWrite(transferPump,HIGH);
             
          }else if (proceed == LOW){
            
            phase = HLTHeat;
            needProceed = false;
            digitalWrite(transferPump,LOW);
            
          }else{
            lcd.setCursor(0,2);
            lcd.print(timer);
          }
          break;
        
        case HLTHeat:
          if (statePhase != "Mash - HLT Heatup"){
            
             clearLine(0);
             clearLine(2);
             clearLine(3);
             statePhase = "Mash - HLT Heatup";
             lcd.setCursor(0,0);
             lcd.print(statePhase);
             sprintf(tempDisplay,"TARGET: %d   ",int(spargeTempTarget));
             lcd.setCursor(0,2);
             lcd.print(tempDisplay);
             
          }else if (needProceed && proceed == LOW){
            
            phase = Sparge;
            needProceed = false;
            mashStartMillis = millis()/1000;
            
          }else{
            
            if(hltTemp >= (spargeTempTarget * 0.95))
             {
               lcd.setCursor(0,3);
               lcd.print("Press 2 to Sparge");
               digitalWrite(readyLED,HIGH);
               needProceed = true;
               
             }else{
               digitalWrite(readyLED,LOW);
               needProceed = false;
             }
             
          }
          break;
        
        case Sparge:
          mashTimer = currentMillis - mashStartMillis;
          tmrMin = mashTimer / 60;
          tmrSec = mashTimer % 60;
          sprintf(timer,"Time %02d:%02d     ",tmrMin,tmrSec);
        
          if (statePhase != "Mash - Sparging"){
            
             clearLine(0);
             clearLine(2);
             clearLine(3);
             statePhase = "Mash - Sparging";
             lcd.setCursor(0,0);
             lcd.print(statePhase);
             lcd.setCursor(0,3);
             lcd.print("Press 2 to Boil");
             digitalWrite(readyLED,LOW);
             delay(1000);//Delay before cutting pump on as the system needs to settle before cutting on pump due to amp pull
             digitalWrite(transferPump,HIGH);
             
          }else if (proceed == LOW){
            
            digitalWrite(transferPump,LOW);
            changeCurrentState();
            needProceed = false;
            
          }else{
            lcd.setCursor(0,2);
            lcd.print(timer);
          }
          break;
          
        default:
          break;
      }
      break;
      
    case Boil:
      //Only update when temp requested
      if (gotTemp){
        wortTemp = getTemperature(wortThermo);
        mashTemp = getTemperature(chillerThermo);
        hltTemp = 0.0;
      }
      
      //LCD Print temp hot water pot
      sprintf(tempDisplay,"Wrt: %d CHLR: %d ",int(wortTemp), int(mashTemp));
      
      lcd.setCursor(0,1);
      lcd.print(tempDisplay);
    
      switch (phase){
        
        case HeatBoilWater:
          if (statePhase != "Boil - Heat Water"){
            
             clearLine(0);
             clearLine(2);
             clearLine(3);
             statePhase = "Boil - Heat Water";
             lcd.setCursor(0,0);
             lcd.print(statePhase);
             sprintf(tempDisplay,"TARGET: %d   ",int(boilTemp));
             lcd.setCursor(0,2);
             lcd.print(tempDisplay);
             delay(500); //Delay in case someone is holding the button down
             
          }else if (needProceed && (proceed == LOW)){
            
            phase = BoilWort;
            boilStartMillis = millis()/1000;
            needProceed = false;
            digitalWrite(readyLED,LOW);
            clearLine(2);
            
          }
          
          if (wortTemp >= (boilTemp * 0.95)){
            
             digitalWrite(readyLED,HIGH);
             needProceed = true;
             //LCD tell to start boil timer
             lcd.setCursor(0,3);
             lcd.print("Press 2 -Start Timer");
             
          }else{
            
             digitalWrite(readyLED,LOW);
          }
          
          break;
          
        case BoilWort:
          boilTimer = currentMillis - boilStartMillis;
          boilTimer = boilLengthSec - boilTimer;
          tmrMin = boilTimer / 60;
          tmrSec = boilTimer % 60;
          sprintf(timer,"Time Left %02d:%02d     ",tmrMin,tmrSec);
          lcd.setCursor(0,2);
          lcd.print(timer);
          
          if (statePhase != "Boil - Boil Wort"){
            
             clearLine(0);
             clearLine(3);
             statePhase = "Boil - Boil Wort";
             lcd.setCursor(0,0);
             lcd.print(statePhase);
             hopIndex = 0;
             delay(500); //Delay in case someone is holding the button down
             
          }else if (needProceed && (proceed == LOW)){
            
            phase = CoolWort;
            digitalWrite(readyLED,LOW);
            needProceed = false;
            boilStartMillis = millis()/1000;
            clearLine(2);
            
          }
          
          if (boilTimer <= 0){
            
            digitalWrite(readyLED,HIGH);
            needProceed = true;
            lcd.setCursor(0,3);
            lcd.print("Press 2 to Cool Wort");
            
          }else if ((boilTimer <= hopAdditions[hopIndex]) && (boilTimer >= hopAdditions[hopIndex]-hopAddSec)){
            
            digitalWrite(readyLED,HIGH);
            lcd.setCursor(0,3);
            char t[20];
            sprintf(t,"Add yo hops! %d min ",hopAdditions[hopIndex]/60); 
            lcd.print(t);
            
          }else{
            
            if (boilTimer <= hopAdditions[hopIndex]-hopAddSec)
            {
              if (hopIndex < (hopAddLength-1))
              {
                hopIndex ++;
              }
            }
            
            clearLine(3);
            
            digitalWrite(readyLED,LOW);
          }
          
          break;
          
        case CoolWort:
          boilTimer = currentMillis - boilStartMillis;
          tmrMin = boilTimer / 60;
          tmrSec = boilTimer % 60;
          sprintf(timer,"Time %02d:%02d     ",tmrMin,tmrSec);
          lcd.setCursor(0,2);
          lcd.print(timer);
          
          if (statePhase != "Boil - Cool Wort"){
             clearLine(0);
             clearLine(3);
             statePhase = "Boil - Cool Wort";
             lcd.setCursor(0,0);
             lcd.print(statePhase);
             
             // Turn on Chiller Pump
             delay(1000); //Delay before cutting pump on as the system needs to settle before cutting on pump due to amp pull
             digitalWrite(chillerPump,HIGH);
             
             // Turn on Transfer Pump for Whirlpool 
             delay(1000); //Delay before cutting pump on as the system needs to settle before cutting on pump due to amp pull
             digitalWrite(transferPump,HIGH);
             
             digitalWrite(readyLED,LOW);
             
          }else if (needProceed && (proceed == LOW)){
            phase = TransferWort;
            digitalWrite(readyLED,LOW);
            digitalWrite(chillerPump,LOW);
            digitalWrite(transferPump,LOW);
            needProceed = false;
            
          }else if (wortTemp <= (coolTargetTemp * 1.05)){
             digitalWrite(readyLED,HIGH);
             needProceed = true;
             //LCD tell to dispaly continue to transfer
             lcd.setCursor(0,3);
             lcd.print("Press 2 to continue");
             
          }else{
             digitalWrite(readyLED,LOW);
             needProceed = false;
             sprintf(tempDisplay,"TARGET: %d   ",int(coolTargetTemp));
             lcd.setCursor(0,3);
             lcd.print(tempDisplay);
          }
          break;
          
        case TransferWort:
          if (statePhase != "Boil - Trans Wort"){
             clearLine(0);
             clearLine(2);
             statePhase = "Boil - Trans Wort";
             lcd.setCursor(0,0);
             lcd.print(statePhase);
             clearLine(3);
             digitalWrite(readyLED,LOW);
             lcd.setCursor(0,3);
             lcd.print("Press 2 to Transfer!");
             needProceed = false;
             delay(500); //Delay in case someone is holding the button down
             
          }else if ((proceed == LOW) && needProceed){
            
            digitalWrite(transferPump,LOW);
            changeCurrentState();
            
          }else if ((proceed == LOW) && !needProceed){
            delay(1000); //Delay before cutting pump on as the system needs to settle before cutting on pump due to amp pull
            digitalWrite(transferPump,HIGH);
            needProceed = true;
            clearLine(3);
            lcd.setCursor(0,3);
            lcd.print("Press 2 to finish!");
            
          }
          break;
        default:
          break;
      }
      
      break;
    default:
      // UNKNOWN STATE
      break;
  }
}

