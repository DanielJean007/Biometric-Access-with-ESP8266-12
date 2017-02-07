
/*
 IoT Manager mqtt device client https://play.google.com/store/apps/details?id=ru.esp8266.iotmanager
 Based on Basic MQTT example with Authentication
 PubSubClient library v 1.91.1 https://github.com/Imroy/pubsubclient
  - connects to an MQTT server, providing userdescr and password
  - publishes config to the topic "/IoTmanager/config/deviceID/"
  - subscribes to the topic "/IoTmanager/hello" ("hello" messages from mobile device) 
  Tested with Arduino IDE 1.6.6 + ESP8266 Community Edition v 2.0.0-stable and PubSubClient library v 1.91.1 https://github.com/Imroy/pubsubclient
  ESP8266 Community Edition v 2.0.0-stable have some HTTPS issues. Push notification temporary disabled.
  sketch version : 1.5
  IoT Manager    : any version
  toggle, range, small-badge widgets demo
*/

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <MQTT.h>
#include <PubSubClient.h>


const char *ssid =  "GVT-C4EE";            // cannot be longer than 32 characters!
const char *pass =  "CP1141RM81R";       // WiFi password

String prefix   = "/IoTmanager";     // global prefix for all topics - must be some as mobile device
String deviceID = "dev01-Test";   // thing ID - unique device id in our project
int GPIO2 = 2; // GPIO Number where you have an LED
WiFiClient wclient;

// config for cloud mqtt broker by DNS hostname ( for example, cloudmqtt.com use: m20.cloudmqtt.com - EU, m11.cloudmqtt.com - USA )
String mqttServerName = "m10.cloudmqtt.com";            // for cloud broker - by hostname, from CloudMQTT account data
int    mqttport = 10122;                                // CloudMQTT.com 1**** port
String mqttuser =  "007";                              // from CloudMQTT account data
String mqttpass =  "1234";                              // from CloudMQTT account data
PubSubClient client(wclient, mqttServerName, mqttport); // for cloud broker - by hostname


// config for local mqtt broker by IP address
//IPAddress server(192, 168, 1, 100);                        // for local broker - by address
//int    mqttport = 1883;                                    // default 1883
//String mqttuser =  "";                                     // from broker config
//String mqttpass =  "";                                     // from broker config
//PubSubClient client(wclient, server, mqttport);            // for local broker - by address

String val;
String ids = "";
int oldtime, newtime, pushtime;
int newValue;

const String stat1 = "{\"status\":\"1\"}";
const String stat0 = "{\"status\":\"0\"}";

const int nWidgets = 1;
String stat        [nWidgets];
String sTopic      [nWidgets];
String color       [nWidgets];
String style       [nWidgets];
String badge       [nWidgets];
String widget      [nWidgets];
String descr       [nWidgets];
String page        [nWidgets];
String thing_config[nWidgets];
String id          [nWidgets];
int    pin         [nWidgets];
int    defaultVal  [nWidgets];
bool   inverted    [nWidgets];

// Push notifications
const char* host = "onesignal.com";
WiFiClientSecure httpClient;
const int httpsPort = 443;
String url = "/api/v1/notifications";

void push(String msg) {
  Serial.println("PUSH: try to send push notification...");
  if (ids.length() == 0) {
     Serial.println("PUSH: ids not received, push failed");
     return;
  }
  if (!httpClient.connect(host, httpsPort)) {
     Serial.println("PUSH: connection failed");
     return;
  }
  String data = "{\"app_id\": \"8871958c-5f52-11e5-8f7a-c36f5770ade9\",\"include_player_ids\":[\"" + ids + "\"],\"android_group\":\"IoT Manager\",\"contents\": {\"en\": \"" + msg + "\"}}";
  httpClient.println("POST " + url + " HTTP/1.1");
  httpClient.print("Host:");
  httpClient.println(host);
  httpClient.println("User-Agent: esp8266.Arduino.IoTmanager");
  httpClient.print("Content-Length: ");
  httpClient.println(data.length());
  httpClient.println("Content-Type: application/json");
  httpClient.println("Connection: close");
  httpClient.println();
  httpClient.println(data);
  httpClient.println();
  Serial.println(data);
  Serial.println("PUSH: done.");
}

String setStatus ( String s ) {
  String stat = "{\"status\":\"" + s + "\"}";
  return stat;
}

String setStatus ( int s ) {
  String stat = "{\"status\":\"" + String(s) + "\"}";
  return stat;
}

void initVar() {
  id    [0] = "0";
  page  [0] = "Garagem";  //"Kitchen";
  descr [0] = "Portão";   //"Lights";
  widget[0] = "toggle";
  pin[0] = GPIO2;                                              // GPIO2 - toggle
  defaultVal[0] = 0;                                       // defaultVal status
  inverted[0] = true;
  sTopic[0]   = prefix + "/" + deviceID + "/portao_escada";
  color[0]   = "\"color\":\"green\"";                       // black, blue, green, orange, red, white, yellow (off - grey)
  
  for (int i = 0; i < nWidgets; i++) {
    if (inverted[i]) {
      if (defaultVal[i]>0) {
         stat[i] = setStatus(0);
      } else {
         stat[i] = setStatus(1);
      }
    } else {
       stat[i] = setStatus(defaultVal[i]);
    }
  }      

  thing_config[0] = "{\"id\":\"" + id[0] + "\",\"page\":\"" + page[0]+"\",\"descr\":\"" + descr[0] + "\",\"widget\":\"" + widget[0] + "\",\"topic\":\"" + sTopic[0] + "\"," + color[0] + "}";   // GPIO switched On/Off by mobile widget toggle
}

// send confirmation
void pubStatus(String t, String payload) {  
    if (client.publish(t + "/status", payload)) { 
       Serial.println("Publish new status for " + t + ", value: " + payload);
    } else {
       Serial.println("Publish new status for " + t + " FAIL!");
    }
}

void pubConfig() {
  bool success;
  success = client.publish(MQTT::Publish(prefix, deviceID).set_qos(1));
  if (success) {
      delay(500);
      for (int i = 0; i < nWidgets; i = i + 1) {
        success = client.publish(MQTT::Publish(prefix + "/" + deviceID + "/config", thing_config[i]).set_qos(1));
        if (success) {
          Serial.println("Publish config: Success (" + thing_config[i] + ")");
        } else {
          Serial.println("Publish config FAIL! ("    + thing_config[i] + ")");
        }
        delay(150);
      }      
  }
  if (success) {
     Serial.println("Publish config: Success");
  } else {
     Serial.println("Publish config: FAIL");
  }
  for (int i = 0; i < nWidgets; i = i + 1) {
      pubStatus(sTopic[i], stat[i]);
      delay(150);
  }      
}

void callback(const MQTT::Publish& sub) {
  Serial.print("Get data from subscribed topic ");
  Serial.print(sub.topic());
  Serial.print(" => ");
  Serial.println(sub.payload_string());

  if (sub.topic() == sTopic[0] + "/control") {
      if (sub.payload_string() == "0") {
     newValue = 1; // inverted
     stat[0] = stat0;
  } else {
     newValue = 0;
     stat[0] = stat1;
       }
  digitalWrite(pin[0],newValue);
  pubStatus(sTopic[0], stat[0]);
 } else if (sub.topic() == prefix + "/ids") {
    ids = sub.payload_string();
 } else if (sub.topic() == prefix) {
    if (sub.payload_string() == "HELLO") {
      pubConfig();
    }
 }
}

void setup() {
  initVar();
  WiFi.mode(WIFI_STA);
  pinMode(pin[0], OUTPUT);
  digitalWrite(pin[0],defaultVal[0]);
  // Setup console
  Serial.begin(115200);
  delay(10);
  Serial.println();
  Serial.println();
  Serial.println("MQTT client started.");
  Serial.print("Free heap = ");
  Serial.println(ESP.getFreeHeap());
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("Connecting via WiFi to ");
    Serial.println(ssid);
//    Serial.println("...");

    WiFi.begin(ssid, pass);

    while(WiFi.waitForConnectResult() != WL_CONNECTED)
      Serial.print(".");

//    if (WiFi.waitForConnectResult() != WL_CONNECTED) {
//      return;
//    }

    Serial.println("");
    Serial.println("WiFi connect: Success");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }

  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      Serial.println("Connecting to MQTT server ...");
      bool success;
      if (mqttuser.length() > 0) {
        success = client.connect( MQTT::Connect( deviceID ).set_auth(mqttuser, mqttpass) );
      } else {
        success = client.connect( deviceID );
      }
      if (success) {
        client.set_callback(callback);
        Serial.println("Connect to MQTT server: Success");
        pubConfig();
        client.subscribe(prefix);                 // for receiving HELLO messages
        client.subscribe(prefix + "/ids");        // for receiving IDS  messages
        client.subscribe(sTopic[0] + "/control"); // for receiving GPIO messages

        Serial.println("Subscribe: Success");
      } else {
        Serial.println("Connect to MQTT server: FAIL");   
        delay(1000);
      }
    }

    if (client.connected()) {
      newtime = millis();
      if (newtime - oldtime > 10000) { // 10 sec
        int x = analogRead(pin[3]);
        val = "{\"status\":\"" + String(x)+ "\"}";
        client.publish(sTopic[3] + "/status", val );  // widget 3
        oldtime = newtime;
        if ((millis()-pushtime > 10000) && (x > 100)) {
           String msg = "Kitchen ADC more then 100! (" + String(x) + ")";
           //
           // ESP8266 Community Edition v 2.0.0-stable have some HTTPS issues. Push notification temporary disabled. Please, uncomment next line if you use future versions.
           //
           // push(msg);
           //
           //
           pushtime = millis();
        }
      }
      client.loop();
    }
  }
}
