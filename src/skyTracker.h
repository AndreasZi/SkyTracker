#include <Arduino.h>
#include <math.h>

#define AZI_DIR 26
#define AZI_STEP 25
#define AZI_SLEEP 33
#define AZI_DIRECTION 0

#define ALT_DIR 12
#define ALT_STEP 14
#define ALT_SLEEP 27
#define ALT_SENSOR 21
#define ALT_DIRECTION 1

#define STEPS_PER_REV 21600 //How many Step inpulses are required for one full revolution


class SkyTracker{
    private:
    //constants for calc
    const unsigned int DAY = 86164; //duration of for one full revolution of the earth [s]
    const double DRAD = 2*PI/ STEPS_PER_REV; //conversion: degree to radiant [rad/°]
    const double TRAD = 2*PI/ DAY; //conversion: radiant to degree [°/rad]

    //angle coordinates
    int declination, latitude, curAzimut, curAltitude, azimut, altitude;

    //time coordinates
    volatile unsigned int hourAngle, rightAscensionSum;

    int getAzimut(){
        //function for recalculating azimut
        if (hourAngle>=DAY/2){
            azimut = acos((-sin(latitude*DRAD)*cos(hourAngle*TRAD)+cos(latitude*DRAD)*tan(declination*DRAD))/sqrt(pow(sin(hourAngle*TRAD),2)+pow(-sin(latitude*DRAD)*cos(hourAngle*TRAD)+cos(latitude*DRAD)*tan(declination*DRAD), 2)))/DRAD;
        }
        else{ //angle moved past 180°
            azimut = STEPS_PER_REV - acos((-sin(latitude*DRAD)*cos(hourAngle*TRAD)+cos(latitude*DRAD)*tan(declination*DRAD))/sqrt(pow(sin(hourAngle*TRAD),2)+pow(-sin(latitude*DRAD)*cos(hourAngle*TRAD)+cos(latitude*DRAD)*tan(declination*DRAD), 2)))/DRAD;
        }

        return azimut;
    }

    int getAltitude(){
        //function for recalculating altitude
        altitude = STEPS_PER_REV/4 - acos(sin(latitude*DRAD)*sin(declination*DRAD) + cos(latitude*DRAD)*cos(hourAngle*TRAD)*cos(declination*DRAD))/(DRAD);
        return altitude;
    }

    void track(){
        //function that is called every second to keep coordinates up to date
        hourAngle++;
        rightAscensionSum++;
        while(hourAngle>DAY){
            hourAngle -= DAY;
        }
    }

    public:

    SkyTracker(){
        //Initialisation of the stepper driver pins

        //Setting azimut pins as output
        pinMode(AZI_DIR, OUTPUT);
        pinMode(AZI_STEP, OUTPUT);
        pinMode(AZI_SLEEP, OUTPUT);

        //Setting altitude pins as output
        pinMode(ALT_DIR, OUTPUT);
        pinMode(ALT_STEP, OUTPUT);
        pinMode(ALT_SLEEP, OUTPUT);
        
        //setting binarysensor as input
        pinMode(ALT_SENSOR,  INPUT_PULLUP);

        //putting both motors to sleep
        digitalWrite(AZI_SLEEP, LOW);
        digitalWrite(ALT_SLEEP, LOW);
    }

    // Essential methods
    // setLatitude(): measure the latitude of the observers location.
    // setPosition(): change the target coordinates
    // move(): keep steppermotors on target. Has to be used repeatedly

    void setLatitude(){
        //waking up motors to reduce chance of accidental changes
        digitalWrite(AZI_SLEEP, HIGH);
        digitalWrite(ALT_SLEEP, HIGH);

        //setting counter to 0
        latitude = 0;

        //change direction of altitudal motor to negativ
        digitalWrite(ALT_DIR, !ALT_DIRECTION);

        //moving altitude-axis down until deadpoint is reached, meanwile counting steps taken
        while(digitalRead(ALT_SENSOR)){
            digitalWrite(ALT_STEP, HIGH);
            delay(1);
            digitalWrite(ALT_STEP, LOW);
            delay(1);
            //once loop is finished latitude should be final
            latitude++;
        }

        //current position of the telescope is now (0 0)
        curAzimut = 0;
        curAltitude = 0;
    }

    void setPositionR (unsigned int RIGHT_ASCENSION, unsigned int DECLINATION){
        //updating position variables based on right ascension
        hourAngle = rightAscensionSum - RIGHT_ASCENSION;
        declination = DECLINATION;
        while(hourAngle>DAY){
            hourAngle -= DAY;
        }
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
        
        //attach interrupt routine
    }

    void move(){
        if(curAzimut != azimut){
            //Only moving azimut in positive direction for now, to avoid it moving backwards after crossing 0

            //setting direction to positive
            digitalWrite(AZI_DIR, AZI_DIRECTION);
            
            //starting pulse to step pin
            digitalWrite(AZI_STEP, HIGH);

            //counting pulse
            curAzimut++;

            //if 360° have been crossed start over
            if(curAzimut >= STEPS_PER_REV){
                curAzimut -= STEPS_PER_REV;
            }            
        }

        // keeping up with altitude
        if(curAltitude < altitude){
            //setting direction to positive
            digitalWrite(ALT_DIR, ALT_DIRECTION);

            //starting pulse to step pin
            digitalWrite(ALT_STEP, HIGH);

            //counting pulse
            curAltitude++;
        }
        else if(curAltitude > altitude && curAltitude > 0){
            //setting direction to negative
            digitalWrite(ALT_DIR, !ALT_DIRECTION);

            //starting pulse to step pin
            digitalWrite(ALT_STEP, HIGH);

            //counting pulse
            curAltitude--;
        } 

        //finishing the pulse to the step pins
        delay(1);
        digitalWrite(AZI_STEP, LOW);
        digitalWrite(ALT_STEP, LOW);
        delay(1);
    }




    //Quality of life functions
    //degToint(): convert String of type 3606060 to the internally used units.
    //timeToint(): convert String of type 246060 to the internally used units.
    //printData(): return all coordinates as String


    int degToInt(String str){
        return str.substring(0,3).toInt()*STEPS_PER_REV/360;
    }

    unsigned int timeToInt(String str){
        return str.substring(0,2).toInt()*3600;
    }
    
    String angle(int arcSeconds){
        int degrees = arcSeconds*360/STEPS_PER_REV;
        int arcMinutes = arcSeconds - degrees*STEPS_PER_REV/360;
        return String(degrees) + "°" + String(arcMinutes) + "'";
    }

    String time(unsigned int seconds){
        int hours = seconds/3600;
        int minutes = (seconds-hours*3600)/60;
        seconds -= hours*3600 + minutes*60;
        return String(hours) + "h" + String(minutes) + "m" + String(seconds) + "s";
    }


    String printData(){
        return "\n\n<-------SkyTracker-Data------->\n"
            "Given:\n"
            "declination: " + angle(declination) + "; hourAngle: " + time(hourAngle) + "\n\n"
            "Calculated target:\n"
            "azimut: " + angle(azimut) + "; altitude: " + angle(altitude) + "\n\n"
            "current position:\n"
            "curAzimut: " + angle(curAzimut) + "; curAltitude: " + angle(curAltitude) + "\n\n"
            "<----------------------------->\n";
    }
};