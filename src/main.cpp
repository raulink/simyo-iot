// Import required libraries
#include <Arduino.h>
#include <ESP8266WiFi.h>

/// MQTT
#include <PubSubClient.h>

#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "LittleFS.h"
#include <Arduino_JSON.h>
//#include <AsyncElegantOTA.h>
#include "../lib/OTA/AsyncElegantOTA.h"


// Replace with your network credentials
String ssid;     //="Raulink";
String password; //="c0nd0m1n10.";
String hostname = "nodo1";
IPAddress ip;
IPAddress gateway; //(192, 168, 0, 1);
IPAddress subnet;  //(255, 255, 255, 0);
int port;

WiFiClient espClient;
PubSubClient client(espClient);

#define MQTT_ID "demo"
#define NUM_ZONES 1
#define NUM_DIGITAL_OUTPUT_PER_ZONE 5
#define DIGITAL_OUTPUTS_OFFSET 0
const int digitalOutputs[NUM_ZONES][NUM_DIGITAL_OUTPUT_PER_ZONE] = {
    {2, 3, 4}
};

#define NUM_ANALOG_OUTPUTS_PER_ZONE 1
#define ANALOG_OUTPUTS_OFFSET 5
const int analogOutputs[NUM_ZONES][NUM_ANALOG_OUTPUTS_PER_ZONE] = {
    {}
};

#define NUM_DIGITAL_INPUTS_PER_ZONE 7
#define DIGITAL_INPUTS_OFFSET 0
const int digitalInputs[NUM_ZONES][NUM_DIGITAL_INPUTS_PER_ZONE] = {
    {1, 2, 3}};

#define NUM_ANALOG_INPUTS_PER_ZONE 7
#define ANALOG_INPUTS_OFFSET 0
#define ANALOG_INPUTS_THRESHOLD 5
const int analogInputs[NUM_ZONES][NUM_ANALOG_INPUTS_PER_ZONE] = {
    {2, 0, 1},
};

// byte mac[] = {0xDE, 0xED, 0xBA, 0xFE, 0xFE, 0xAE};

/// Sector MQTT
#define MQTT_ID "demo"
IPAddress broker;      // Direccion IP del broker
int brokerport = 1883; // Puerto del broker

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Create a WebSocket object
AsyncWebSocket ws("/ws");

// Set number of outputs
#define NUM_OUTPUTS 4

// Assign each GPIO to an output
int outputGPIOs[NUM_OUTPUTS] = {2, 4, 12, 14};

/**
 * @brief Funcion lectura archivo JSON
 *
 */
void readJSON(File file)
{
  String json;
  if (!file)
  {
    Serial.println("Failed to open file for reading");
    return;
  }
  Serial.println("File Content:");
  while (file.available())
  {
    json = file.readString();
  }
  String s;
  JSONVar objeto = JSON.parse(json);

  // Obtener ssid
  s = JSON.stringify(objeto["ssid"]);
  s.replace("\"", "");
  ssid = s;

  //  Obtener Pass
  s = JSON.stringify(objeto["pass"]);
  s.replace("\"", "");
  password = s;

  // Obtener IP
  s = JSON.stringify(objeto["ip"]);
  s.replace("\"", "");
  ip.fromString(s);

  // Obtener y transformar IP
  s = JSON.stringify(objeto["ip"]);
  s.replace("\"", "");
  ip.fromString(s);

  // Obtener y transformar IP
  s = JSON.stringify(objeto["gateway"]);
  s.replace("\"", "");
  gateway.fromString(s);

  // Obtener y transformar IP
  s = JSON.stringify(objeto["subnet"]);
  s.replace("\"", "");
  subnet.fromString(s);

  // Obtener y transformar IP de broker
  s = JSON.stringify(objeto["subnet"]);
  s.replace("\"", "");
  broker.fromString(s);

  file.close();
}

// Initialize LittleFS
void initLittleFS()
{
  if (!LittleFS.begin())
  {
    Serial.println("An error has occurred while mounting LittleFS");
  }

  readJSON(LittleFS.open("/config.json", "r"));

  // JsonObject& root = jsonBuffer.parseObject(json);
  // Serial.println("LittleFS mounted successfully");
}
// Initialize WiFi
void initWiFi()
{
  WiFi.mode(WIFI_STA);
  WiFi.config(ip, gateway, subnet);
  WiFi.hostname(hostname.c_str());

  Serial.print("Connecting to WiFi ..");
  Serial.print(ssid);
  Serial.print(password);
  Serial.printf("Hostname: %s\n", WiFi.hostname().c_str()); // Muestra nombre de host, verificar si ingresa por nombre

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
}

String getOutputStates()
{
  JSONVar myArray;
  for (int i = 0; i < NUM_OUTPUTS; i++)
  {
    myArray["gpios"][i]["output"] = String(outputGPIOs[i]);
    myArray["gpios"][i]["state"] = String(digitalRead(outputGPIOs[i]));
  }
  String jsonString = JSON.stringify(myArray);
  return jsonString;
}

void notifyClients(String state)
{
  ws.textAll(state);
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len)
{
  AwsFrameInfo *info = (AwsFrameInfo *)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
  {
    data[len] = 0;
    if (strcmp((char *)data, "states") == 0)
    {
      notifyClients(getOutputStates());
    }
    else
    {
      int gpio = atoi((char *)data);
      digitalWrite(gpio, !digitalRead(gpio));
      notifyClients(getOutputStates());
    }
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len)
{
  switch (type)
  {
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

void initWebSocket()
{
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

/**
 * @brief funcion callback MQTT
 *
 */
void callback(String topic, byte *message, unsigned int length)
{
  String messageTemp;
  /* Serial.print("Mensaje recibido en topic:");
  Serial.print(topic);
  Serial.print(".Mensaje:"); */

  for (int i = 0; i < length; i++)
  {
    // Serial.print((char) message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  // Mas funciones añadidas para el control de mas GPIOS con MQTT

  // Si el mensaje se recibe en el topico especifico, ver el mensaje
  if (topic == "maq/mep1/z1")
  {
    Serial.print("Cambiar estado z1");
    if (messageTemp == "on")
    {
      digitalWrite(4, HIGH);
      Serial.print("On");
    }
    else if (messageTemp == "off")
    {
      digitalWrite(4, LOW);
      Serial.print("Off");
    }
  }
  Serial.println();
}

void reconnect()
{
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    /*
     YOU MIGHT NEED TO CHANGE THIS LINE, IF YOU'RE HAVING PROBLEMS WITH MQTT MULTIPLE CONNECTIONS
     To change the ESP device ID, you will have to give a new name to the ESP8266.
     Here's how it looks:
       if (client.connect("ESP8266Client")) {
     You can do it like this:
       if (client.connect("ESP1_Office")) {
     Then, for the other ESP:
       if (client.connect("ESP2_Garage")) {
      That should solve your MQTT multiple connections problem
    */
    if (client.connect(MQTT_ID))
    {
      Serial.println("connected");
      // Subscribe or resubscribe to a topic
      // You can subscribe to more topics (to control more LEDs in this example)
      client.subscribe("room/lamp");
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

/**
 * @brief Funcion Setup
 *
 */
void setup()
{
  // Serial port for debugging purposes
  Serial.begin(115200);

  // Set GPIOs as outputs
  for (int i = 0; i < NUM_OUTPUTS; i++)
  {
    pinMode(outputGPIOs[i], OUTPUT);
  }

  client.setServer(broker, brokerport);
  client.setCallback(callback);

  initLittleFS();
  initWiFi();
  initWebSocket();

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(LittleFS, "/index.html", "text/html", false); });

  server.serveStatic("/", LittleFS, "/");

  // Start ElegantOTA
  AsyncElegantOTA.begin(&server);

  // Start server
  server.begin();
}

void loop()
{
  /* const char *tempz1 = "22";
  const char *humz1 = "32"; */
  ws.cleanupClients();

  if (!client.connected())
    reconnect();
  if (!client.loop())
    client.connect(MQTT_ID);

  // Si existe alguna salida analogica o digital
  /* client.publish("temp/z1",tempz1);
  client.publish("hum/z1",humz1); */
}