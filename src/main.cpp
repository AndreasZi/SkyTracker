#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include "secrets.h" 
#include "skyTracker.h"

SkyTracker sky = SkyTracker();

AsyncWebServer server(80);
const String index_html = 
        "<!DOCTYPE html>"
        "<html>"
            "<head>"
                "<script>"
                    "async function requestState (path){var x = await fetch(path);};"  
                    "function setPosition(){requestState('data?hourAngle='+document.getElementById('hourAngle').value + '&declination='+document.getElementById('declination').value);};"
                    "function setLatitude(){requestState('setLatitude');};"
                "</script>"
            "</head>"
            "<body>"
                "<p>"
                "<a href=\"setLatitude\">setLatitude</a><br><br>"
                "Stundenwinkel<br><input id='hourAngle' type='text';\"></input><br><br>"
                "Deklination<br><input id='declination' type='text';\"></input><br><br>"
                "<button onclick=\"setPosition();\">setPosition</button><br><a id='info'></a>"
                "</p>"
            "</body>"
        "</html>";






void setup() {

  Serial.begin(9600);
  WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    }

    //main page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/html",  index_html);
    });

    server.on("/setLatitude", HTTP_GET, [](AsyncWebServerRequest *request){
        Serial.print("setting Latitude");
        sky.setLatitude();
        Serial.print("Latitude is now Set");
        request->send(200, "text/html",  index_html);
    });

    //data-api for live-updates
    server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request){
        unsigned long hourAngle = 0;
        long declination = 0;
        /* if(request->hasParam("hourAngle")) {
            hourAngle=request->getParam("hourAngle")->value().toInt();
            Serial.println(hourAngle);
        }
        if(request->hasParam("declination")) {
            declination=request->getParam("declination")->value().toInt();
            Serial.println(declination);
        } */
        if(request->hasParam("hourAngle")) {
            //Accepting string with xxhxxmxxs format
            String str = request->getParam("hourAngle")->value();
            //converting string to seconds
            hourAngle = sky.timeToInt(str);
        }
        if(request->hasParam("declination")) {
            //Accepting string with +xx°xx' format
            String str = request->getParam("declination")->value();
            declination = sky.degToInt(str);
        }
        sky.setPositionH(hourAngle, declination);
        Serial.println(sky.printData());
        request->send(200, "text/plain", " ");
        
    });

    server.begin();
    /**/
    Serial.begin(9600);
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    /* sky.setLatitude();
    Serial.println("Latitude set!"); */
}

void loop() {
  sky.move();
  // put your main code here, to run repeatedly:
}

