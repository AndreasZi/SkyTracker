#include <Arduino.h>
#include <math.h>

#define _TIMERINTERRUPT_LOGLEVEL_ 4
#include "ESP32TimerInterrupt.h"
#define TIMER0_INTERVAL_MS 1000

#define AZI_DIR 26
#define AZI_STEP 25
#define AZI_SLEEP 33
#define AZI_DIRECTION 0

#define ALT_DIR 12
#define ALT_STEP 14
#define ALT_SLEEP 27
#define ALT_DIRECTION 1

#define STEPS_PER_REV 21600 //How many Step inpulses are required for one full revolution

ESP32Timer ITimer0(0);
class SkyTracker{
    private:
    //constants fpr Calc
    const unsigned long DAY = 86164; //duration of one day [ms]
    const double DRAD = 2*PI/ STEPS_PER_REV; //conversion: degree to radiant [rad/°]
    const double TRAD = 2*PI/ DAY; //conversion: radiant to degree [°/rad]
    long declination = STEPS_PER_REV/4, latitude = 43*STEPS_PER_REV/360, curAzimut, curAltitude, azimut, altitude;
    volatile unsigned long hourAngle = DAY/2;

    long getAzimut(){
        //function for calculating azimut
        if (hourAngle<STEPS_PER_REV/2){
            return acos((sin(latitude*DRAD)*cos(hourAngle*TRAD)+cos(latitude*DRAD)*tan(declination*DRAD))/sqrt(pow(sin(hourAngle),2)+(sin(latitude*DRAD)*cos(hourAngle))))/DRAD;
        }
        else{ //angle moved past 180°
            return STEPS_PER_REV - acos((sin(latitude*DRAD)*cos(hourAngle*TRAD)+cos(latitude*DRAD)*tan(declination*DRAD))/sqrt(pow(sin(hourAngle*TRAD),2)+(sin(latitude*DRAD)*cos(hourAngle*TRAD))))/DRAD;
        }
    }

    long getAltitude(){
        //function for calculating altitude
        return STEPS_PER_REV/4 - acos(sin(latitude*DRAD)*tan(declination*DRAD) - cos(latitude*DRAD)*cos(hourAngle*TRAD)*cos(declination*DRAD))/DRAD;
    }

    bool IRAM_ATTR TimerHandler0(void * timerNo){
        //function that is being called by timing interrupt
        hourAngle++;
        //azimut = getAzimut();
        //altitude = getAltitude();
        return true;
    }

    

    public:

    SkyTracker(){
        //calculate the starting position based on lattitude
        azimut = getAzimut();
        altitude = getAltitude();
        curAzimut = azimut;
        curAltitude = altitude;

        //Initialisation of the stepper driver pins

        //Setting azimut pins as output
        pinMode(AZI_DIR, OUTPUT);
        pinMode(AZI_STEP, OUTPUT);
        pinMode(AZI_SLEEP, OUTPUT);
        digitalWrite(AZI_SLEEP, LOW);

        //Setting altitude pins as output
        pinMode(ALT_DIR, OUTPUT);
        pinMode(ALT_STEP, OUTPUT);
        pinMode(ALT_SLEEP, OUTPUT);
        digitalWrite(ALT_SLEEP, LOW);

        //creating setup for timer functions
        

    }

    // Essential methods
    // setPosition(): change the target coordinates
    // move(): keep steppermotors on target

    void setPosition (unsigned long HOUR_ANGLE, unsigned long DECLINATION){
        //updating position variables
        hourAngle = HOUR_ANGLE;
        declination = DECLINATION;
        azimut = getAzimut();
        altitude = getAltitude();

        //wake up driver boards
        digitalWrite(AZI_SLEEP, HIGH);
        digitalWrite(ALT_SLEEP, HIGH);

        //ITimer0.attachInterruptInterval(TIMER0_INTERVAL_MS * 1000, SkyTracker::TimerHandler0);
    }

    void move(){
        if(curAzimut != azimut){
            
            //Only moving azimut in positive direction for now, to avoid it moving backwards after crossing 0
            digitalWrite(AZI_DIR, AZI_DIRECTION);
            digitalWrite(AZI_STEP, HIGH);
            curAzimut++;
            if(curAzimut >= STEPS_PER_REV){
                curAzimut -= STEPS_PER_REV;
            }            
        }
        /* else if(curAzimut > azimut){
            digitalWrite(AZI_DIR, !AZI_DIRECTION);
            digitalWrite(AZI_STEP, HIGH);
            curAzimut--;
        } */
        
        

        // keeping up with altitude
        if(curAltitude < altitude){
            digitalWrite(ALT_DIR, ALT_DIRECTION);
            digitalWrite(ALT_STEP, HIGH);
            curAltitude++;
        }
        else if(curAltitude > altitude){
            digitalWrite(ALT_DIR, !ALT_DIRECTION);
            digitalWrite(ALT_STEP, HIGH);
            curAltitude--; 
        } 
        
        

        delay(1);
        digitalWrite(AZI_STEP, LOW);
        digitalWrite(ALT_STEP, LOW);
        delay(1);
    }

    //Quality of life functions
    //degToLong(): convert String of type 3606060 to the internally used units.
    //timeToLong(): convert String of type 246060 to the internally used units.
    //printData(): print all coordinates to the Serial Monitor


    long degToLong(String str){
        
        /* if(str[0]=='-'){
            return -(str.substring(1,3).toInt()*60 + str.substring(4,6).toInt());
        }
        else{
            {
            return str.substring(1,3).toInt()*60 + str.substring(4,6).toInt();
        }
        } */
        /* Serial.print("str, substring 0,3: ");
        Serial.print(str);
        Serial.print(", ");
        Serial.println(str.substring(0,3)); */
        return str.substring(0,3).toInt()*STEPS_PER_REV/360;
    }

    unsigned long timeToLong(String str){
        //return str.substring(0,2).toInt()*3600+str.substring(3,5).toInt()*60+str.substring(6,8).toInt();
        return str.substring(0,2).toInt()*3600;
    }
    
    long degree(long minAngle){
        return minAngle*360/STEPS_PER_REV;
    }

    int hours(unsigned long seconds){
        return seconds/3600;
    }


    void printData(){
        Serial.println("<------SkyTracker-Data------>");

        Serial.println("Given values:");
        Serial.print("declination: ");
        Serial.print(degree(declination));
        Serial.print("; hourAngle: ");
        Serial.println(hours(hourAngle));

        Serial.println("resulting values:");
        Serial.print("azimut: ");
        Serial.print(degree(azimut));
        Serial.print("; altitude: ");
        Serial.println(degree(altitude));

        Serial.println("current position:");        
        Serial.print("curAzimut: ");
        Serial.print(degree(curAzimut));
        Serial.print("; curAltitude: ");
        Serial.println(degree(curAltitude));
        Serial.println("<--------------------------->");
    }

};