#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include "A4988.h"

#include "skyTracker.h"


#define AZI_DIR 26
#define ALT_DIR 16

#define AZI_STEP 25
#define ALT_STEP 17

#define AZI_SLEEP 33
#define ALT_SLEEP 5

#define ALT_SENSOR 23

#define STEPS_PER_REV 21600 


//On the prototype, all driver-MST-Pins are connected to one pin on the ESP.
#define MST 32

A4988 stepperA(STEPS_PER_REV, AZI_DIR, AZI_STEP, AZI_SLEEP);
A4988 stepperH(STEPS_PER_REV, ALT_DIR, ALT_STEP, ALT_SLEEP);



SkyTracker sky = SkyTracker(stepperA, stepperH);

AsyncWebServer server(80);
const String index_html = 
        "<head>"
            "<script>"
                    "async function requestState (path){var x = await fetch(path);};"  
                    "function setPositionH(){requestState('data?hourAngle='+document.getElementById('hourAngle').value + '&declination='+document.getElementById('declination').value);};"
                    "function setPositionR(){requestState('data?rightAscension='+document.getElementById('rightAscension').value + '&declination='+document.getElementById('declination').value);};"
                    "function toggleTracking(){requestState('toggleTracking');};"
                    "function setSideral(){requestState('data/setSideral?rightAscension='+document.getElementById('rightAscension').value);};"
                "</script>"
                "<style>.category{padding: 1em;float: left;width: 100%;}</style>"
            "</head>"
            "<body>"
                    "<div class='category'>Deklination <input id='declination' type='text';></input></div>"
                    "<div class='category'>Stundenwinkel <input id='hourAngle' type='text';></input><button onclick='setPositionH();'>setPositionH</button></div>"
                    "<div class='category'>Right Ascension <input id='rightAscension' type='text';></input><button onclick='setPositionR();'>setPositionR</button><button onclick='setSideral();'>setSideral</button></div>"                    
                    "<div class='category'><button onclick='toggleTracking();'>toggleTracking</button></div>"
            "</body>";






void setup() {

    /* stepperA.setMicrostep(16);
    stepperH.setMicrostep(16); */

    pinMode(ALT_SENSOR,OUTPUT);
    digitalWrite(ALT_SENSOR,HIGH);
    Serial.begin(9600);
  
    IPAddress local_IP(192,168,1,1);
    IPAddress gateway(192,168,1,0);
    IPAddress subnet(255,255,255,0);
    
    WiFi.softAPConfig(local_IP, gateway, subnet);
    WiFi.softAP("SkyTracker");
    //main page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/html",  index_html);
    });

    server.on("/data/setSideral", HTTP_GET, [](AsyncWebServerRequest *request){
        if(request->hasParam("rightAscension") && request->hasParam("hourAngle")) {
            //Accepting string with xxhxxmxxs format
            sky.setSideral(sky.timeToInt(request->getParam("rightAscension")->value()),sky.timeToInt(request->getParam("hourAngle")->value()));
        }
        request->send(200, "text/html",  index_html);
    });

    server.on("/toggleTracking", HTTP_GET, [](AsyncWebServerRequest *request){
        sky.toggleTracking();
        request->send(200, "text/html",  index_html);
    });  

    

    //data-api for live-updates
    server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request){
        unsigned int hourAngle = 0;
        unsigned int rightAscension = 0;
        int declination = 0;
        /* if(request->hasParam("hourAngle")) {
            hourAngle=request->getParam("hourAngle")->value().toInt();
            Serial.println(hourAngle);
        }
        if(request->hasParam("declination")) {
            declination=request->getParam("declination")->value().toInt();
            Serial.println(declination);
        } */
        if(request->hasParam("declination")) {
                //Accepting string with +xx°xx' format
                String str = request->getParam("declination")->value();
                declination = sky.degToInt(str);
        }
        if(request->hasParam("hourAngle")) {
            //Accepting string with xxhxxmxxs format
            String str = request->getParam("hourAngle")->value();
            //converting string to seconds
            hourAngle = sky.timeToInt(str);
        
            if(request->hasParam("declination")) {
                //Accepting string with +xx°xx' format
                String str = request->getParam("declination")->value();
                declination = sky.degToInt(str);
            }
            sky.setPositionH(hourAngle, declination);
            Serial.println(sky.printData());
        }

        if(request->hasParam("rightAscension")) {
            //Accepting string with xxhxxmxxs format
            String str = request->getParam("rightAscension")->value();
            //converting string to seconds
            rightAscension = sky.timeToInt(str);
        
            
            sky.setPositionR(rightAscension, declination);
            Serial.println(sky.printData());
        }
        request->send(200, "text/plain", " ");
        
    });

    server.begin();
    /**/
    Serial.begin(9600);
    Serial.print("IP Address: ");
    Serial.println(WiFi.softAPIP());
    

    //To set your current latitude, target the sysem at the polar star and hit the altitude endstop
    pinMode(ALT_SENSOR,  INPUT_PULLUP);
    while(digitalRead(ALT_SENSOR));
    delay(500);
    sky.setLatitude(ALT_SENSOR); //setting lat via wifi takes to much time for the interrupt

    Serial.print("setLat");
}

void loop() {
  sky.move();
  // put your main code here, to run repeatedly:
}

