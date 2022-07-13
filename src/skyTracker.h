#include <Arduino.h>
#include <math.h>
#include "BasicStepperDriver.h"

#define RPM 1

#define SECOND 1

#define ALT_DIRECTION 1
#define AZI_DIRECTION 1





//https://techtutorialsx.com/2017/10/07/esp32-arduino-timer-interrupts/  
hw_timer_t * timer;
portMUX_TYPE timerMux;
volatile int timerCounter;  

void IRAM_ATTR track(){
        //function that is called every second to keep coordinates up to date
        portENTER_CRITICAL_ISR(&timerMux);
        timerCounter+=SECOND;
        portEXIT_CRITICAL_ISR(&timerMux);
    }



class SkyTracker{
    private:
    //constants for calc
    static const unsigned int DAY = 86164; //duration of for one full revolution of the earth [s]
    static constexpr double TRAD = 2*PI/ DAY; //conversion: radiant to degree [°/rad]

    unsigned int STEPS; //How many Step inpulses are required for one full revolution
    double DRAD; //conversion: degree to radiant [rad/°]

    BasicStepperDriver *stepperA, *stepperH;

    bool tracking = false;

    //angle coordinates
    int declination, latitude;
    unsigned int hourAngle, sidereal, curAzimut, curAltitude, azimut, altitude;
    

    int getAzimut(){
        //function for recalculating azimut
        if (hourAngle>=DAY/2){
            azimut = acos((-sin(latitude*DRAD)*cos(hourAngle*TRAD)+cos(latitude*DRAD)*tan(declination*DRAD))/sqrt(pow(sin(hourAngle*TRAD),2)+pow(-sin(latitude*DRAD)*cos(hourAngle*TRAD)+cos(latitude*DRAD)*tan(declination*DRAD), 2)))/DRAD;
        }
        else{ //angle moved past 180°
            azimut = STEPS - acos((-sin(latitude*DRAD)*cos(hourAngle*TRAD)+cos(latitude*DRAD)*tan(declination*DRAD))/sqrt(pow(sin(hourAngle*TRAD),2)+pow(-sin(latitude*DRAD)*cos(hourAngle*TRAD)+cos(latitude*DRAD)*tan(declination*DRAD), 2)))/DRAD;
        }
        
        return azimut;
    }

    int getAltitude(){
        //function for recalculating altitude
        altitude = STEPS/4 - acos(sin(latitude*DRAD)*sin(declination*DRAD) + cos(latitude*DRAD)*cos(hourAngle*TRAD)*cos(declination*DRAD))/(DRAD);
        
        return altitude;
    }


    
    
    

    public:

    SkyTracker(BasicStepperDriver &stepperAzimut, BasicStepperDriver &stepperAltitude){
        //Initialisation of the stepper driver pins

        //Setting steppermotors
        stepperA = &stepperAzimut;
        stepperH = &stepperAltitude;
        stepperA->begin(RPM);
        stepperH->begin(RPM);

        STEPS = stepperA->getSteps();
        DRAD = 2*PI/ STEPS;

        //Microstepping is always on
        /* pinMode(AZI_MST, OUTPUT);
        pinMode(ALT_MST, OUTPUT);
        digitalWrite(AZI_MST, HIGH);
        digitalWrite(ALT_MST, HIGH); */

        
        


        //Setting up timer Interrupt
        timerMux = portMUX_INITIALIZER_UNLOCKED;
        timer = timerBegin(0, 80, true);
        timerAttachInterrupt(timer, &track, true);
        timerAlarmWrite(timer, 1000000, true);
    }

    // Essential methods
    // setLatitude(): measure the latitude of the observers location.
    // setPosition(): change the target coordinates
    // move(): keep steppermotors on target. Has to be used repeatedly

    void setLatitude(uint8_t sensorPin){
        //setting binarysensor as input
        pinMode(sensorPin,  INPUT_PULLUP);
        
        //waking up motors to reduce chance of accidental changes
        stepperA->enable();
        stepperH->enable();

        //setting counter to 0
        latitude = 0;

        

        //moving altitude-axis down until deadpoint is reached, meanwile counting steps taken
        while(digitalRead(sensorPin)){
            stepperH->move(-ALT_DIRECTION);
            //once loop is finished latitude should be final
            latitude++;
            delay(1);
        }

        //current position of the telescope is now (0 0)
        curAzimut = 0;
        curAltitude = 0;
    }


    void setSideral(unsigned int RIGHT_ASCENSION, unsigned int HOUR_ANGLE){
        //Setting sideral is required in order to use the setPositionR function
        sidereal = HOUR_ANGLE - RIGHT_ASCENSION;
    }


    void setPositionR (unsigned int RIGHT_ASCENSION, unsigned int DECLINATION){
        //updating position variables based on right ascension
        
        hourAngle = sidereal - RIGHT_ASCENSION;
        while(hourAngle>DAY){
            hourAngle -= DAY;
        }
        

        declination = DECLINATION;
        //recalculating target azimut and altitude
        getAzimut();
        getAltitude();
        
        //attach interrupt routine
    }

    void setPositionH (unsigned int HOUR_ANGLE, unsigned int DECLINATION){
        //updating position variables based on hour angle
        hourAngle = HOUR_ANGLE;

        declination = DECLINATION;

        //recalculating target azimut and altitude
        getAzimut();
        getAltitude();
    }

    void toggleTracking(){
        //toggling the systems function to follow the movement of the sky
        tracking = !tracking;
        if(tracking){
            timerCounter = 0;
            timerAlarmEnable(timer);
        }
        else{
            timerAlarmDisable(timer);
        }

    }

    void move(){
        if(timerCounter>0){
            //refreshing timeing variables with countervalues
            hourAngle+=timerCounter;
            sidereal+=timerCounter;

            //limiting timing variables to one day
            while(hourAngle>DAY){
                hourAngle -= DAY;
            }
            while(sidereal>DAY){
                sidereal -= DAY;
            }

            //Reseting timer counter
            portENTER_CRITICAL(&timerMux);
            timerCounter = 0;
            portEXIT_CRITICAL(&timerMux);

            

            //recalculating target position
            getAzimut();
            getAltitude();
        }
        if(azimut - curAzimut < 0 && azimut - curAzimut > -STEPS-10){
            //allowing backwards movement for faster targeting time

            //Move stepper to the desired location  
            stepperA->move(azimut - curAzimut);
            //setnew location
            curAzimut=azimut;
        }
        else if(azimut - curAzimut < -STEPS-10 && azimut - curAzimut >= -STEPS){
            //avoid it moving backwards after crossing 0

            //Move stepper to the desired location  
            stepperA->move(azimut - curAzimut + STEPS);
            //setnew location
            curAzimut=azimut;
        }
        else if(curAzimut != azimut){
            //Only moving azimut in positive direction

            //Move stepper to the desired location  
            stepperA->move(azimut - curAzimut);
            //setnew location
            curAzimut=azimut;     
        }

        if(curAltitude != altitude){
            //Move stepper to the desired location  
            stepperH->move(altitude - curAltitude);
            //setnew location
            curAltitude = altitude;
        }
    delay(1); //delay for stability
    }




    //Quality of life functions
    //degToint(): convert String of type 3606060 to the internally used units.
    //timeToint(): convert String of type 246060 to the internally used units.
    //printData(): return all coordinates as String


    int degToInt(String str){
        //converts input string of type 360°60m to steps
        /* int d = 0, m = 0;
        if (str.indexOf('°')>0 ){
            d = str.substring((str.indexOf('°')-4<0)?0:str.indexOf('°')-4,str.indexOf('°')-1).toInt()*STEPS/360;
        }
        if (str.indexOf('m')>0){
            m = str.substring(str.indexOf('m')-3,str.indexOf('m')-1).toInt()*STEPS/(360*60) - d;
        } */
        
        return str.substring(0,2).toInt()*STEPS/360;
        //(str[0]=='-')?-(d + m):d + m;
    }

    unsigned int timeToInt(String str){
        //converts inputr string of type 24h60m60s to seconds
        
        int begin = str.indexOf('h')-2;
        return str.substring(begin,2).toInt()*3600 + str.substring(begin+3,2).toInt()*60+ str.substring(begin+6,2).toInt();
    }
    
    String angle(int arcSeconds){
        int degrees = arcSeconds/60/60;
        int arcMinutes = (arcSeconds - degrees*60*60)/60;
        return String(degrees) + "°" + String(arcMinutes) + "'";
    }

    String time(unsigned int seconds){
        int hours = seconds/3600;
        int minutes = (seconds-hours*3600)/60;
        seconds -= hours*3600 + minutes*60;
        return String(hours) + "h" + String(minutes) + "m" + String(seconds) + "s";
    }


    String printData(){
        return "\n<-------SkyTracker-Data------->\n"
            "Equatorial[Declination: " + angle(declination) + ", Hour Angle: " + time(hourAngle) + "]\n"
            "Horizontal[Azimut: " + angle(azimut) + ", Altitude: " + angle(altitude) + "]\n";
    }
};