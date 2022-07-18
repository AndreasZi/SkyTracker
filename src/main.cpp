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
                    "function setSideral(){requestState('data/setSideral?rightAscension='+document.getElementById('rightAscension').value + '&hourAngle='+document.getElementById('hourAngle').value);};"
                "</script>"
                "<style>.category{padding: 1em;float: left;width: 100%;}</style>"
            "</head>"
            "<body>"
                    "<div class='category'>Deklination <input id='declination' type='number';></input></div>"
                    "<div class='category'>Stundenwinkel <input id='hourAngle' type='number';></input><button onclick='setPositionH();'>setPositionH</button></div>"
                    "<div class='category'>Right Ascension <input id='rightAscension' type='number';></input><button onclick='setPositionR();'>setPositionR</button><button onclick='setSideral();'>setSideral</button></div>"                    
                    "<div class='category'><button onclick='toggleTracking();'>toggleTracking</button></div>"
            "</body>";






void setup() {

    Serial.begin(9600);
  

    //Setting up Wifi AP
    IPAddress local_IP(192,168,1,1);
    IPAddress gateway(192,168,1,0);
    IPAddress subnet(255,255,255,0);
    
    WiFi.softAPConfig(local_IP, gateway, subnet);
    WiFi.softAP("SkyTracker");


  
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        //Tghis serves the mainpage on adress 192.168.1.1/
        request->send(200, "text/html",  index_html);
    });

    server.on("/data/setSideral", HTTP_GET, [](AsyncWebServerRequest *request){
        //function for calculating sideral time
        if(request->hasParam("rightAscension") && request->hasParam("hourAngle")) {
            
            sky.setSideral(request->getParam("rightAscension")->value().toFloat()*3600,request->getParam("hourAngle")->value().toFloat()*3600);
        }
        Serial.println("Sideral set");
        request->send(200, "text/html",  index_html);
    });

    
    server.on("/toggleTracking", HTTP_GET, [](AsyncWebServerRequest *request){
        //activating or deactivating tracking function
        sky.toggleTracking();
        request->send(200, "text/html",  index_html);
    });  

    

    
    server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request){
        //Function for setting coordinates
        //Declaring and assigning variables to avoid problems with datatransfer
        unsigned int hourAngle = 0;
        unsigned int rightAscension = 0;
        int declination = 0;
        
        if(request->hasParam("declination")) {
            declination = request->getParam("declination")->value().toFloat()*60;
        }
        if(request->hasParam("hourAngle")) {
            //Position can be set using hourAngle
            hourAngle = request->getParam("hourAngle")->value().toFloat()*3600;
        
            
            sky.setPositionH(hourAngle, declination);
            //Printing positions for reference
            Serial.println(sky.printData());
        }

        if(request->hasParam("rightAscension")) {
            //Position can be set using rightascension
            rightAscension = request->getParam("rightAscension")->value().toFloat()*60;
    
            sky.setPositionR(rightAscension, declination);
            //Printing positions for reference
            Serial.println(sky.printData());
        }
        request->send(200, "text/plain", " ");
        
    });


    server.begin();
    Serial.begin(9600);
    Serial.print("IP Address: ");
    Serial.println(WiFi.softAPIP());
    

    //To set your current latitude, target the sysem at the polar star and press the altitude endstop
    //setting lat via wifi takes to much time for the wifi-routine
    pinMode(ALT_SENSOR,  INPUT_PULLUP);
    while(digitalRead(ALT_SENSOR));
    delay(500);
    sky.setLatitude(ALT_SENSOR); 

    Serial.print("Latitude ist now set");
}

void loop() {
  sky.move();//movement if required
  // put your main code here, to run repeatedly:
}

