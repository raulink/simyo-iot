// Import required libraries
#include <Arduino.h>
#include <ESP8266WiFi.h>

#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "LittleFS.h"
#include <Arduino_JSON.h>
#include <AsyncElegantOTA.h>

// Replace with your network credentials
 String ssid;//="Raulink"; 
 String password;//="c0nd0m1n10.";
 IPAddress ip;
 IPAddress gateway;//(192, 168, 0, 1);
 IPAddress subnet;//(255, 255, 255, 0);
 int port;
 

// const size_t capacity = JSON_OBJECT_SIZE(4) + 70;
//DynamicJsonBuffer jsonBuffer(capacity);

//const char* json = "{\"ssid\":\"Raulink\",\"pass\":\"c0nd0m1n10.\",\"ip\":\"192.168.0.50\",\"port\":\"8080\"}";

//JsonObject& root = jsonBuffer.parseObject(json);

/* const char* ssid = root["ssid"]; // "Raulink"
const char* password = root["password"]; // "c0nd0m1n10."
const char* ip = root["ip"]; // "192.168.0.50"
const char* port = root["port"]; // "8080" */

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Create a WebSocket object
AsyncWebSocket ws("/ws");

// Set number of outputs
#define NUM_OUTPUTS  4

// Assign each GPIO to an output
int outputGPIOs[NUM_OUTPUTS] = {2, 4, 12, 14};

/**
 * @brief Funcion lectura archivo JSON
 * 
 */
void readJSON(File file){    
  String json;
  if(!file){
    Serial.println("Failed to open file for reading");
    return;
  }
  Serial.println("File Content:");
  while(file.available()){    
     json = file.readString();     
  }
  String s;
  JSONVar objeto = JSON.parse(json);

  // Obtener ssid
  s = JSON.stringify( objeto["ssid"]);  
  s.replace("\"","");
  ssid = s;

  //  Obtener Pass
  s = JSON.stringify( objeto["pass"]);
  s.replace("\"","");
  password = s;
  
  //Obtener IP
  s = JSON.stringify( objeto["ip"]);
  s.replace("\"","");
  ip.fromString(s);
   
  // Obtener y transformar IP
  s = JSON.stringify( objeto["ip"]);
  s.replace("\"","");
  ip.fromString(s);
  
  // Obtener y transformar IP
  s = JSON.stringify( objeto["gateway"]);
  s.replace("\"","");
  gateway.fromString(s);

  // Obtener y transformar IP
  s = JSON.stringify( objeto["subnet"]);
  s.replace("\"","");
  subnet.fromString(s);

  file.close();
}

// Initialize LittleFS
void initLittleFS() {
  if (!LittleFS.begin()) {    
    Serial.println("An error has occurred while mounting LittleFS");
  }
  
  readJSON(LittleFS.open("/config.json","r"));
  
  //JsonObject& root = jsonBuffer.parseObject(json);
  //Serial.println("LittleFS mounted successfully");
}
// Initialize WiFi
void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.config(ip,gateway,subnet);  
  
  Serial.print("Connecting to WiFi ..");  
  Serial.print(ssid);
  Serial.print(password);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
}

String getOutputStates(){
  JSONVar myArray;
  for (int i =0; i<NUM_OUTPUTS; i++){
    myArray["gpios"][i]["output"] = String(outputGPIOs[i]);
    myArray["gpios"][i]["state"] = String(digitalRead(outputGPIOs[i]));
  }
  String jsonString = JSON.stringify(myArray);
  return jsonString;
}

void notifyClients(String state) {
  ws.textAll(state);
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    if (strcmp((char*)data, "states") == 0) {
      notifyClients(getOutputStates());
    }
    else{
      int gpio = atoi((char*)data);
      digitalWrite(gpio, !digitalRead(gpio));
      notifyClients(getOutputStates());
    }
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

void setup(){
  // Serial port for debugging purposes
  Serial.begin(115200);
  
  // Set GPIOs as outputs

  for (int i =0; i<NUM_OUTPUTS; i++){
    pinMode(outputGPIOs[i], OUTPUT);
  }
  initLittleFS();
  initWiFi();
  initWebSocket();

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/index.html", "text/html",false);
  });

  server.serveStatic("/", LittleFS, "/");

  // Start ElegantOTA
  AsyncElegantOTA.begin(&server);
  
  // Start server
  server.begin();
}

void loop() {
  ws.cleanupClients();
}