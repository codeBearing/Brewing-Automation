# Brewing-Automation

###QuikBrew
QuikBrew is a semi-automated brew contoller written for the Arduino. 

#####QuikBrew2 for the Uno includes support for the following features: 
   1 transfer pump <br>
   1 immersion chiller pump<br>
   3 DS18B20 Temperature Sensors<br>
   1 LCD Display 4x20<br>
   2 Buttons<br>
   3 LEDs<br>
   
   For recipe modification you will have to change the settings and rebuild sketcht.
   
#####QuikBrew2 for the Mega includes support for the following features: <br>
   1 transfer pump <br>
   1 immersion chiller pump<br>
   3 DS18B20 Temperature Sensors<br>
   1 LCD Display 4x20<br>
   2 Buttons<br>
   3 LEDs<br>
   SD Card Reader for Recipe Loading<br>
   
   Recipe Example(Time in s and Temp in F):<br>
    HLTMASHTEMP=154<br>
    HLTSPRGTEMP=175<br>
    MASHLENGTHSEC=3600<br>
    SPRGSTARTHEATSEC=600<br>
    HLTBOILTEMP=200<br>
    BOILLENGTHSEC=3600<br>
    COOLWORTTEMP=70<br>
    HOPADD1=3600<br>
    HOPADD2=1800<br>
    HOPADD3=600<br>
    HOPADD4=0<br>
    HOPADD5=-1 (-1 is no hop addition)<br>
    HOPADD6=-1<br>
    HOPADD7=-1<br>
    HOPADD8=-1<br>
    HOPADD9=-1<br>
    HOPADD10=-1<br>
    
##### Needed 3rd Party Libraries:<br>
   OneWire <br>
   DallasTemperature <br>
   LiquidCrystal_I2C <br>

### Fermenation Logger
The fermenation logger will log 2 temperatures and 1 humidity reading. The current use is a DS18B20 to monitor wort temperature and a DHT11 to monitor the fermentor temperature and humdity. It currenty will log the collected data every 30 seconds to and csv file stored on an SD Card. (Sorry no Fritzing schematic - maybe in the future)

##### Hardware used:
Arduino Uno <br>
Adafruit Data Logging sheild (http://www.adafruit.com/products/1141) <br>
DS18B20 <br>
DHT11 <br>

##### Needed 3rd Party Libraries:
OneWire <br>
DallasTemperature <br>
DHT <br>
RTClib <br>
