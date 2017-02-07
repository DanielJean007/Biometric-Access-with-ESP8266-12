#include <MQTT.h>
#include <PubSubClient.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>

#include "FPS_GT511C3.h"  //Lib specially designed for FPS GT511C3, but we use with the *C1 module.
#include "SoftwareSerial.h"

//Defines
#define RxFPS 4     //Rx FPS module + ESP12
#define TxFPS 5     //Tx FPS module + ESP12
#define SET_AP 14   //ESP12
#define WIFI_ON 10  //ESP12
#define FAIL 12     //Red LED
#define SUCCESS 13  //Green LED and Relay to open Door

#define timeOff 300
#define timeErr 500
#define timeOpenDoor 1500
#define timeBlink 200

//Global Vars --Shame on me.
bool masterFinger = false;    //The person in control of entering Add, deleteID and DeleteDB mode
bool AP_setup = false;        //Button that enables Access Point configuration
bool triggerPressed = false;  //Aux button for AP_setup
bool wifi_onPressed = false;  //Button that enables Wifi Connection
bool special_mode = false;    //Mode where one person enables all id's to be allowed entrance

String prefix   = "/IoTmanager";      // global prefix for all topics - must be some as mobile device
String deviceID = String( ESP.getChipId() ) + "-" + String( ESP.getFlashChipId() );     // IoT thing device ID - unique device id in our project
String string_status;

// config for cloud mqtt broker by DNS hostname ( for example, cloudmqtt.com use: m20.cloudmqtt.com - EU, m11.cloudmqtt.com - USA )
String mqttServerName = "m10.cloudmqtt.com";            // for cloud broker - by hostname, from CloudMQTT account data
int    mqttport = 10122;                                // CloudMQTT.com 1**** port
String mqttuser =  "user-userName";                     // from CloudMQTT account data
String mqttpass =  "your-passwrd";
//config for cloud mqtt broker by DNS hostname ( for example, cloudmqtt.com use: m20.cloudmqtt.com - EU, m11.cloudmqtt.com - USA )

//For buttons and MQTT stuff
int freeheap;
const int nWidgets = 2; // number of widgets
String sTopic      [nWidgets];
String stat        [nWidgets];
int    pin         [nWidgets];
String thing_config[nWidgets];
//For buttons and MQTT stuff

//Function definitions
//Functions for MQTT
void actionClose();   //Topic to closing the Door 
void actionOpen();    //Topic to opening the Door 
String setStatus ( String s );
String setStatus ( int s );
void initVar();
void pubStatus(String t, String payload);
void pubConfig();
void callback(const MQTT::Publish& sub);

//FPS Functions
void scanFinger();
void Enroll();
void enrollMaster();
void identifyUser();
void connectClient();
void openOnMQTTandAccess();

//Output functions
void blinkLED_specialMode(int timeSet, bool toBlink);
void blinkLED(int timeSet, bool toBlink);
void pwmLED();
void openDoor(int timeSet);

//Wifi configuration functions
bool WifiSetupHandler();

void FreeHEAP();

//Functions initializations
FPS_GT511C3 fps(RxFPS, TxFPS);
WiFiClient wclient;
PubSubClient client(wclient, mqttServerName, mqttport); // for cloud broker - by hostname
StaticJsonBuffer<1024> jsonBuffer;
JsonObject& json_status = jsonBuffer.createObject();

bool debugMode = false;  //To see messagens on Arduino IDE Serial Monitor
void setup() {
  fps.UseSerialDebug = false; //To see messagens from FPS library

  initVar();
  
  if(debugMode){
    Serial.begin(115200);
    delay(10);
    WiFi.printDiag(Serial);
    Serial.println();
    Serial.println();
    Serial.println("MQTT client started.");
  }
  
  pinMode(FAIL,OUTPUT);
  pinMode(SUCCESS,OUTPUT);
  pinMode(SET_AP, INPUT_PULLUP);
  pinMode(WIFI_ON, INPUT_PULLUP);


  FreeHEAP();
  freeheap = 100000;

  fps.Open();
  fps.SetLED(true);

  if(digitalRead(WIFI_ON) == LOW) WiFi.begin();
}

void loop() {
  fps.SetLED(true); //Turn the pad back on again.
  masterFinger = fps.CheckEnrolled(0);

  //The sketch will run infinitely into enrollMaster() until
  //the master finger is setup.
  if(!masterFinger) enrollMaster();

  //Identifying user if the pad is pressed or if the special mode is enabled.
  if (fps.IsPressFinger()) identifyUser();

  //Blink LED to show the system is alive
  if(!special_mode) blinkLED(timeBlink, true);
  if(special_mode) blinkLED_specialMode(timeBlink*2, true);

  //Checking if the user wants to setup AP
  triggerPressed = false;
  wifi_onPressed = false;

  if(digitalRead(SET_AP) == LOW)
    triggerPressed = true;
  if(digitalRead(WIFI_ON) == LOW)
    wifi_onPressed = true;

  if(triggerPressed && wifi_onPressed)
      AP_setup = WifiSetupHandler();

  if (WiFi.status() == WL_CONNECTED &&  wifi_onPressed)
    AP_setup = true;

  //Connecting to MQTT in case we have internet connection.
  if (WiFi.status() == WL_CONNECTED && AP_setup && wifi_onPressed){
    if (!client.connected()) connectClient();
    if (client.connected()) client.loop();
  }
}

void FreeHEAP() {
  if ( ESP.getFreeHeap() < freeheap ) {
    if ( ( freeheap != 100000) ) {
      if(debugMode){
        Serial.print("Memory leak detected! old free heap = ");
        Serial.print(freeheap);
        Serial.print(", new value = ");
        Serial.println(ESP.getFreeHeap());
      }
    }
    freeheap = ESP.getFreeHeap();
  }
}

String setStatus ( String s ) {
  json_status["status"] = s;
  string_status = "";
  json_status.printTo(string_status);
  return string_status;
}

String setStatus ( int s ) {
  json_status["status"] = s;
  string_status = "";
  json_status.printTo(string_status);
  return string_status;
}

void initVar() {
  // widget0 - This widget is just a label to indicate if the door is open or close
  JsonObject& root = jsonBuffer.createObject();
  sTopic[0] = prefix + "/" + deviceID + "/Estado";
  stat  [0] = setStatus ("Fechado");
  root["id"] = 0;
  root["page"] = "buttons";
  root["widget"] = "anydata";
  root["class1"] = "item no-border";                          // class for 1st div
  root["style1"] = "";                                        // style for 1st div
  root["descr"]  = "Acesso está:";                            // text  for description
  root["class2"] = "assertive";                                // class for description from Widgets Guide - Color classes
  root["style2"] = "font-size:20px;float:left;font-weight:bold;"; // style for description
  root["topic"] = sTopic[0];
  root["class3"] = "light assertive-bg padding-left padding-right rounded";;                                        // class for 3 div - SVG
  root["style3"] = "float:right;font-weight:bold;";                           // style for 3 div - SVG
  root["height"] = "40";                                      // SVG height without "px"
  root["color"]  = "#52FF00";                                 // color for active segments
  root["inactive_color"] = "#414141";                         // color for inactive segments
  root.printTo(thing_config[0]);

  // widget1 - This widget is a slider button to open the door.
  //This widget will return to its closed state after 'timeOpenDoor' seconds
  JsonObject& root2 = jsonBuffer.createObject();
  pin   [1] = SUCCESS;   // SUCCESS - LED attached
  sTopic[1] = prefix + "/" + deviceID + "/toggle";
  stat  [1] = setStatus(0); // LED off at startup
  root2["id"] = 1;
  root2["page"] = "buttons";
  root2["descr"]  = "Abre/Fecha Acesso: ";                   // text  for description
  root2["widget"] = "toggle";
  root2["color"] = "green";                     // black, blue, green, orange, red, white, yellow (off - grey)
  root2["topic"] = sTopic[1];
  root2.printTo(thing_config[1]);
  pinMode(pin[1], OUTPUT); // PIN 13
}

void pubStatus(String t, String payload) {
  if (client.publish(t + "/status", payload)) {
    if(debugMode) Serial.println("Publish new status to " + t + "/status" + ", value: " + payload);
  } else {
    if(debugMode) Serial.println("Publish new status to " + t + "/status" + " FAIL!");
  }
  
  FreeHEAP(); // check memory leak
}

void pubConfig() {
  bool success;
  success = client.publish(MQTT::Publish(prefix, deviceID).set_qos(1));
  if (success) {
    delay(100);
    for (int i = 0; i < nWidgets; i = i + 1) {
      success = client.publish(MQTT::Publish(prefix + "/" + deviceID + "/config", thing_config[i]).set_qos(1));
      if (success) {
        if(debugMode) Serial.println("Publish config: Success (" + thing_config[i] + ")");
      } else {
        if(debugMode) Serial.println("Publish config FAIL! ("    + thing_config[i] + ")");
      }
      delay(100);
    }
  }
  if (success) {
    if(debugMode) Serial.println("Publish config: Success");
  } else {
    if(debugMode) Serial.println("Publish config: FAIL");
  }

  for (int i = 0; i < nWidgets; i = i + 1) {
    pubStatus(sTopic[i], stat[i]);
    delay(100);
  }
}

void callback(const MQTT::Publish& sub) {
  if(debugMode){
    Serial.print("Get data from subscribed topic ");
    Serial.print(sub.topic());
    Serial.print(" => ");
    Serial.println(sub.payload_string());
  }

  if ( sub.payload_string() == "HELLO" ) {  // handshaking
    pubConfig();
  }

  if (sub.topic() == sTopic[0] + "/control") {   // control from toggle - widget id 0 - ON
    // payload from button always 1, change status only for widget toggle - id 2
    stat[1] = setStatus(1);   // 1-on 2-off LED on phone
    digitalWrite(pin[1], 1);  // 0-on 1-off LED on device
    pubStatus(sTopic[1], stat[1]);
  } else if (sub.topic() == sTopic[0] + "/control") {   // control from simple-btn - widget id 1 - OFF
    // payload from button always 1, change status only for widget toggle - id 2
    stat[1] = setStatus(0);  // 1-on 2-off LED on phone
    digitalWrite(pin[1], 0); // 0-on 1-off LED on device
    pubStatus(sTopic[1], stat[1]);
  } else if (sub.topic() == sTopic[1] + "/control") {  // control from simple-btn - widget id 2 - ON
    // payload from button always 1, change status only for widget toggle - id 2
    if (sub.payload_string() == "0") {
      actionClose();
      digitalWrite(pin[1], 0);
    } else {  //Open the gate via the app on the mobile
              //After some time the access is closed.
        openOnMQTTandAccess()
    }
    pubStatus(sTopic[0], stat[0]);
    pubStatus(sTopic[1], stat[1]);
  }
}

void actionClose(){
  stat[1] = setStatus(0);
  stat[0] = setStatus("Fechado");
}

void actionOpen(){
  stat[1] = setStatus(1);
  stat[0] = setStatus("Aberto");
}

void scanFinger(){
  /*
   * This method handles when the finger master is detected.
   * One can delete a user or add a new user.
   * After the finger master is detected, the other user must press the finger on the pad.
   * If the other user was already in the database they will be deleted,
   * Otherwise, they will be added to the DB.
  */

  fps.SetLED(true); //Turn the pad back on again.
  if(debugMode) Serial.println("Press finger");
  while(fps.IsPressFinger() == false) delay(100);
  fps.CaptureFinger(false);
  int id = fps.Identify1_N();

  fps.SetLED(false);  //To signal the user they can remove the finger from the pad.
  if(debugMode) Serial.println("Remove finger.");
  delay(timeOff);

  if (id <20)
  {
    //Delete ID.
    if(id == 0){
      if(debugMode) Serial.println("Deleting DB...");
      else digitalWrite(FAIL, LOW);   // turn the LED on (HIGH is the voltage level)

      if(fps.DeleteAll()) {
        if(debugMode) Serial.println("DB has been erased!");
        else pwmLED();
      }

    }else{
      if(fps.DeleteID(id)){
        if(debugMode) Serial.println("ID deleted graciously.");
        else{
          pwmLED();
        }
      }else{
        if(debugMode) Serial.println("Could not delete ID.");
        else blinkLED(timeErr, true);
      }
    }
  }
  else
  {
    Enroll();
  }
}

void Enroll(){
  /*
   * This method directly uses the lib to enroll a new user.
  */
  // find open enroll id
  int enrollid = 0;
  bool usedid = true;

  //Looking for an empty address to store the new user.
  while (usedid == true && enrollid < 20){
    usedid = fps.CheckEnrolled(enrollid);
    if (usedid==true) enrollid++;
  }
  fps.EnrollStart(enrollid);

  // enroll
  fps.SetLED(true); //Turn the pad back on again.
  if(enrollid < 20){
    if(debugMode) {
      Serial.print("Press finger to Enroll #");
      Serial.println(enrollid);
    }else{
      digitalWrite(FAIL, HIGH);   // turn the LED on (HIGH is the voltage level)
    }

    while(fps.IsPressFinger() == false) delay(100);
    bool bret = fps.CaptureFinger(true);
    int iret = 0;
    if (bret != false){

      fps.SetLED(false);  //To signal the user they can remove the finger from the pad.
      if(debugMode) Serial.println("Remove finger.");
      delay(timeOff);
      fps.SetLED(true); //Turn the pad back on again.

      fps.Enroll1();

      while(fps.IsPressFinger() == true) delay(100);
      if(debugMode) Serial.println("Press same finger again");
      while(fps.IsPressFinger() == false) delay(100);
      bret = fps.CaptureFinger(true);
      if (bret != false){
        fps.SetLED(false);  //To signal the user they can remove the finger from the pad.
        if(debugMode) Serial.println("Remove finger.");
        delay(timeOff);
        fps.SetLED(true); //Turn the pad back on again.

        fps.Enroll2();

        while(fps.IsPressFinger() == true) delay(100);
        if(debugMode) Serial.println("Press same finger for the last time");

        while(fps.IsPressFinger() == false) delay(100);
        bret = fps.CaptureFinger(true);

        if (bret != false)
        {
          fps.SetLED(false);  //To signal the user they can remove the finger from the pad.
          if(debugMode) Serial.println("Remove finger.");
          delay(timeOff);
          fps.SetLED(true); //Turn the pad back on again.

          iret = fps.Enroll3();
          if (iret == 0)
          {
            if(debugMode) Serial.println("Enrolling Successfull");
            else pwmLED();
            fps.SetLED(false);
          }
          else
          {
            if(debugMode){
              Serial.print("Enrolling Failed with error code:");
              Serial.println(iret);
            }else{
              blinkLED(timeErr, true);
            }
          }
        }
        else{
          if(debugMode) Serial.println("Failed to capture third finger");
          else {
            blinkLED(timeErr, true);
          }
        }
      }
      else{
        if(debugMode) Serial.println("Failed to capture second finger");
        else {
          blinkLED(timeErr, true);
          }
      }
    }
    else{
      if(debugMode) Serial.println("Failed to capture first finger");
      else {
        blinkLED(timeErr, true);
      }
    }
  }
}

void blinkLED(int timeSet, bool toBlink){
  digitalWrite(FAIL, HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(timeSet);              // wait for a second
  digitalWrite(FAIL, LOW);    // turn the LED off by making the voltage LOW
  if(toBlink)delay(timeSet);
}

void pwmLED(){
  // fade in from min to max in increments of 5 points:
  for (int fadeValue = 0 ; fadeValue <= 255; fadeValue++) {
    // sets the value (range from 0 to 255):
    analogWrite(FAIL, fadeValue);
    // wait for 30 milliseconds to see the dimming effect
    delay(5);
  }

  // fade out from max to min in increments of 5 points:
  for (int fadeValue = 255 ; fadeValue >= 0; fadeValue--) {
    // sets the value (range from 0 to 255):
    analogWrite(FAIL, fadeValue);
    // wait for 30 milliseconds to see the dimming effect
    
    delay(5);
  }
}

void openDoor(int timeSet){
  digitalWrite(SUCCESS, HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(timeSet);              // wait for a second
  digitalWrite(SUCCESS, LOW);    // turn the LED off by making the voltage LOW

}

bool WifiSetupHandler(){
  if(debugMode) Serial.println("\n Entering Wifi setup");

    WiFiManager wifiManager;

//    reset settings - for testing only
//    wifiManager.resetSettings();

    wifiManager.setTimeout(120);  //The user has 2 minutes to setup the AP

    AP_setup = wifiManager.startConfigPortal("Biometric Access - Set Access Point");

    if (!AP_setup){
      if(debugMode) Serial.println("failed to connect and hit timeout");
      delay(1000);
    }

    //if you get here you have connected to the WiFi
    if(debugMode) Serial.println("Leaving wifi setup...)");

    return AP_setup;
}

void enrollMaster(){
  if(debugMode) Serial.println("Please, enter master finger.");
  else digitalWrite(FAIL, HIGH);   // turn the LED on (HIGH is the voltage level)

  if(debugMode) Serial.println("Please, place master finger on the pad.");
  else pwmLED();  //To signal entering in the scaning master finger mode.

  Enroll();                 //Enrolling in case the system doesn't have the master finger.

  masterFinger = fps.CheckEnrolled(0);
  if(masterFinger){
    if(debugMode) Serial.println("Master finger enrolled successfuly.");
    else pwmLED();  //To signal leaving the scaning master finger mode.
  }else{
    enrollMaster();
  }
}

void identifyUser(){
    fps.CaptureFinger(false);
    int id = fps.Identify1_N();

    fps.SetLED(false);  //To signal the user they can remove the finger from the pad.
    if(debugMode) Serial.println("Remove finger.");
    delay(timeOff);

    if (id <20){  //20 'cause it's the GT511C1
      if(id == 0 && masterFinger){
        if(!debugMode) digitalWrite(FAIL, HIGH);   // turn the LED on (HIGH is the voltage level)

        if(debugMode) Serial.println("Enter a ID stored to delete it, or a new ID to enroll.");
        scanFinger(); //Handles the finger master access.

        if(!debugMode) digitalWrite(FAIL, LOW);   // turn the LED on (HIGH is the voltage level)
      }else{
        if(debugMode){
          Serial.print("Verified ID:");
          Serial.println(id);
        }
        
        if(id > 1){
          if(debugMode) Serial.println("Open Access:");
          openOnMQTTandAccess();
        }
        
        if(id == 1 && !special_mode){  //The special user that allows any id to be granted access.
          if(debugMode) Serial.println("Open Access and Special Mode:");
          special_mode = true;
          openOnMQTTandAccess();
        }else if(id == 1 && special_mode){  //The special user that allows any id to be granted access.
          if(debugMode) Serial.println("Leaving Special Mode:");
          special_mode = false;
        }
      }
    }
    else
    if (!special_mode){
      if(debugMode) Serial.println("Finger not found");
      else blinkLED(timeErr, false);
    }else{
      openOnMQTTandAccess();
    }
}

void connectClient(){
  if(debugMode) Serial.println("Connecting to MQTT server ...");
  bool success;
  if (mqttuser.length() > 0) {
    success = client.connect( MQTT::Connect( deviceID ).set_auth(mqttuser, mqttpass) );
  } else {
    success = client.connect( deviceID );
  }
  if (success) {
    client.set_callback(callback);
    if(debugMode) Serial.println("Connect to MQTT server: Success");
    client.subscribe(prefix);  // for receiving HELLO messages and handshaking
    client.subscribe(prefix + "/" + deviceID + "/+/control"); // subscribe to all "control" messages for all widgets of this device
    pubConfig();
  } else if(debugMode){
    Serial.println("Connect to MQTT server: FAIL");
    delay(1000);
  }
}

void openOnMQTTandAccess(){
  actionOpen();
  
  pubStatus(sTopic[0], stat[0]);
  pubStatus(sTopic[1], stat[1]);

  openDoor(timeOpenDoor);
  actionClose();
  
  pubStatus(sTopic[0], stat[0]);
  pubStatus(sTopic[1], stat[1]);
}

void blinkLED_specialMode(int timeSet, bool toBlink){
  digitalWrite(FAIL, HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(timeSet*2);              // wait for a second
  digitalWrite(FAIL, LOW);    // turn the LED off by making the voltage LOW
  if(toBlink)delay(timeSet/2);
}

