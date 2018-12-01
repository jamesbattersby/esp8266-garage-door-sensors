//-----------------------------------------------------------------------------
// Garage Door Sensors
// By James Battersby
//-----------------------------------------------------------------------------

#include <Arduino.h>
// For encryption
#include <xxtea-lib.h>
// For OTA
#include <ArduinoOTA.h>
// For MQTT messaging
#include <PubSubClient.h>
// Config for WiFi
#include "wifiConfig.h"

#define DOOR_1 D1 // Digital IO pin for door 1
#define DOOR_2 D2 // Digital IO pin for door 2

#define CLOSED false // Value when door closed
#define OPEN true  // Value when door open

// Local functions
void notify(int, bool);
void setUpWifi();
void callback(char*, byte*, unsigned int);
void connectToMqtt();

WiFiClient espClient;
PubSubClient mqttClient(espClient);

//-----------------------------------------------------------------------------
// setup
//
// Configure the serial port and IO pins.  Send initial notification.
//-----------------------------------------------------------------------------
void setup() {
  Serial.begin(9600);
  setUpWifi();
  pinMode(DOOR_1, INPUT_PULLUP);
  pinMode(DOOR_2, INPUT_PULLUP);
  notify(1, CLOSED);
  notify(2, CLOSED);
}

//-----------------------------------------------------------------------------
// loop
//
// The main loop.  Check the status of the doors every second.
//-----------------------------------------------------------------------------
void loop() {
  static bool door1_state = CLOSED;
  static bool door2_state = CLOSED;

  if (!mqttClient.connected())
  {
    connectToMqtt();
  }
  mqttClient.loop();
  ArduinoOTA.handle();

  bool state = digitalRead(DOOR_1) ? OPEN : CLOSED;
  if (state != door1_state)
  {
    door1_state = state;
    notify(1, door1_state);
  }

  state = digitalRead(DOOR_2) ? OPEN : CLOSED;
  if (state != door2_state)
  {
    door2_state = state;
    notify(2, door2_state);
  }

  delay(1000);
}

//-----------------------------------------------------------------------------
// notify
//
// Send notification of the new door state to the MQTT server, and to the
// serial port.
//-----------------------------------------------------------------------------
void notify(int door, bool state)
{
  char message[20];
  sprintf(message, "%d:%s", door, state == OPEN ? "open" : "closed");
  printf("%s\n", message);
  mqttClient.publish("garageDoors", message);
}

//-----------------------------------------------------------------------------
// setUpWifi
//
// Responsible for connecting to Wifi, initialising the over-air-download
// handlers.
//
// If GENERATE_ENCRYPTED_WIFI_CONFIG is set to true, will also generate
// the encrypted wifi configuration data.
//-----------------------------------------------------------------------------
void setUpWifi()
{
  String ssid = SSID;
  String password = WIFI_PASSWORD;

  // Set the key
  xxtea.setKey(ENCRYPTION_KEY);

  // Perform Encryption on the Data
#if GENERATE_ENCRYPTED_WIFI_CONFIG
  Serial.printf("--Encrypted Wifi SSID: %s\n", xxtea.encrypt(SSID).c_str());
  Serial.printf("--Encrypted Wifi password: %s\n", xxtea.encrypt(WIFI_PASSWORD).c_str());
  Serial.printf("--Encrypted MQTT username: %s\n", xxtea.encrypt(MQTT_USERNAME).c_str());
  Serial.printf("--Encrypted MQTT password: %s\n", xxtea.encrypt(MQTT_PASSWORD).c_str());
#endif // GENERATE_ENCRYPTED_WIFI_CONFIG

  unsigned char pw[MAX_PW_LEN];
  unsigned char ss[MAX_PW_LEN];
  // Connect to Wifi
  WiFi.mode(WIFI_STA);
  xxtea.decrypt(password).getBytes(pw, MAX_PW_LEN);
  xxtea.decrypt(ssid).getBytes(ss, MAX_PW_LEN);

  WiFi.begin((const char*)(ss), (const char*)(pw));
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(callback);

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

//-----------------------------------------------------------------------------
// connectToMqtt
//
// Connect to the MQTT server
//-----------------------------------------------------------------------------
void connectToMqtt()
{
  unsigned char mqttUser[MAX_PW_LEN];
  unsigned char mqttPassword[MAX_PW_LEN];
  String username = MQTT_USERNAME;
  String passwordmqtt = MQTT_PASSWORD;
  xxtea.decrypt(username).getBytes(mqttUser, MAX_PW_LEN);
  xxtea.decrypt(passwordmqtt).getBytes(mqttPassword, MAX_PW_LEN);
  int retry = 20;

  while (!mqttClient.connected() && --retry)
  {
    Serial.println("Connecting to MQTT...");

    if (mqttClient.connect("GarageDoors", reinterpret_cast<const char *>(mqttUser), reinterpret_cast<const char *>(mqttPassword)))
    {
      Serial.println("connected");
    }
    else
    {
      Serial.print("failed with state ");
      Serial.println(mqttClient.state());
      delay(2000);
    }
  }

  if (retry == 0)
  {
    printf("Failed to connect to MQTT server on %s:%d", MQTT_SERVER, MQTT_PORT);
  }
}

//-----------------------------------------------------------------------------
// callback
//
// Process a received mesage.  We are not expected any, but print it out anyway
//-----------------------------------------------------------------------------
void callback(char* topic, byte* payload, unsigned int length)
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < (int)length; i++)
  {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}
