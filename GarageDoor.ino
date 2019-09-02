/*
  NOTE: This isn't the "cleanest" of code! Some of the naming conventions are inconsistent
  and there is a lot of hard-coding. Ideally the certs would be installed into an easily
  updated portion of ROM, perhaps using some of the ideas that the WiFiManager uses for storing
  the WiFi credentials... maybe make it so it listens on a port so the AWS server and credentials
  can be updated (and require a password to be set the first-time)? Then after that, you use the
  password to update the AWS credentials if needed.
  
  AWS IoT Garage Door. Components:

  https://www.amazon.com/gp/product/B01NACU547 "5V D1 mini Relay Shield 5V D1 mini Relay Module for WeMos D1"
  https://www.amazon.com/gp/product/B076F53B6S "IZOKEE D1 Mini NodeMcu Lua 4M Bytes WLAN WiFi Internet Development Board Base on ESP8266 ESP-12F for Arduino, 100% Compatible with WeMos D1 Mini"
  (combined cost of $7.15 including tax, note that these are 3 packs)
  https://www.amazon.com/gp/product/B016YS8B98 "Cat5e Ethernet Bulk Cable"
  ($0.157 per foot, about 20 feet = $3.14, note that this is 250ft)
  https://www.amazon.com/gp/product/B00LYCUSBY "Aleph DC-1561 W Surface Mount Alarm Magnetic Contact" (aka reed switch)
  ($1.70 x 2 = $3.40, note that this is a 10 pack, also only used a single magnet)

  Total cost of build, not including experimental breadboard, solder, soldering iron,
  breadboard jumper cables:
  
  $13.69

  Monthly cost using AWS IoT Core: $1

  Wiring:

  Fully wired together the Relay shield, which defaults to using D1 as the control
  (and only the ground and 5V wires really need to be connected, but all the other
  wires being connected was harmless, so I used the provided brackets which wire
  them all together).
  I then connected two wires from the GRD pin and connected them to each of the
  reed switch sensors.
  The other end (it doesn't matter which) of each reed switch sensor was connected
  to the D2 (for the door all the way open sensor) and D5 (for the door all the way
  open sensor).
  Finally, I used the screw pins on the relay to connect to the "NO" side (normally
  open), and connected those to where my garage door opener button connects. To find
  out which two connections to use, I measured the resistance between two of the wires
  (which was disconnected from the garage door opener) and validated that the
  resistance changed when the door open button was pushed. As a final test, I "jumped"
  these two and validated that the garage door began to open (or close if it was already
  closed).

  To use this file, update clientKeyPem and the xxx-ats.iot.us-east-1.amazonaws.com
  server to your AWS account.
*/

#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <PubSubClient.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager

// Constants:
#define OPENER_RELAY_PIN D1
// NOTE: If door is open during start, the d1 mini won't start!
#define DOOR_OPEN_PIN D2
// DO NOT USE D3 since that is GPIO0 and if it is grounded at boot
// it will enter the ROM serial bootloader for esptool.py
// https://github.com/espressif/esptool/wiki/ESP8266-Boot-Mode-Selection
#define DOOR_CLOSED_PIN D5
const char VeriSign_Class_3_Public_Primary_Certification_Authority_G5[] PROGMEM =
    "-----BEGIN CERTIFICATE-----\n" \
    "MIIE0zCCA7ugAwIBAgIQGNrRniZ96LtKIVjNzGs7SjANBgkqhkiG9w0BAQUFADCB\n" \
    "yjELMAkGA1UEBhMCVVMxFzAVBgNVBAoTDlZlcmlTaWduLCBJbmMuMR8wHQYDVQQL\n" \
    "ExZWZXJpU2lnbiBUcnVzdCBOZXR3b3JrMTowOAYDVQQLEzEoYykgMjAwNiBWZXJp\n" \
    "U2lnbiwgSW5jLiAtIEZvciBhdXRob3JpemVkIHVzZSBvbmx5MUUwQwYDVQQDEzxW\n" \
    "ZXJpU2lnbiBDbGFzcyAzIFB1YmxpYyBQcmltYXJ5IENlcnRpZmljYXRpb24gQXV0\n" \
    "aG9yaXR5IC0gRzUwHhcNMDYxMTA4MDAwMDAwWhcNMzYwNzE2MjM1OTU5WjCByjEL\n" \
    "MAkGA1UEBhMCVVMxFzAVBgNVBAoTDlZlcmlTaWduLCBJbmMuMR8wHQYDVQQLExZW\n" \
    "ZXJpU2lnbiBUcnVzdCBOZXR3b3JrMTowOAYDVQQLEzEoYykgMjAwNiBWZXJpU2ln\n" \
    "biwgSW5jLiAtIEZvciBhdXRob3JpemVkIHVzZSBvbmx5MUUwQwYDVQQDEzxWZXJp\n" \
    "U2lnbiBDbGFzcyAzIFB1YmxpYyBQcmltYXJ5IENlcnRpZmljYXRpb24gQXV0aG9y\n" \
    "aXR5IC0gRzUwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQCvJAgIKXo1\n" \
    "nmAMqudLO07cfLw8RRy7K+D+KQL5VwijZIUVJ/XxrcgxiV0i6CqqpkKzj/i5Vbex\n" \
    "t0uz/o9+B1fs70PbZmIVYc9gDaTY3vjgw2IIPVQT60nKWVSFJuUrjxuf6/WhkcIz\n" \
    "SdhDY2pSS9KP6HBRTdGJaXvHcPaz3BJ023tdS1bTlr8Vd6Gw9KIl8q8ckmcY5fQG\n" \
    "BO+QueQA5N06tRn/Arr0PO7gi+s3i+z016zy9vA9r911kTMZHRxAy3QkGSGT2RT+\n" \
    "rCpSx4/VBEnkjWNHiDxpg8v+R70rfk/Fla4OndTRQ8Bnc+MUCH7lP59zuDMKz10/\n" \
    "NIeWiu5T6CUVAgMBAAGjgbIwga8wDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8E\n" \
    "BAMCAQYwbQYIKwYBBQUHAQwEYTBfoV2gWzBZMFcwVRYJaW1hZ2UvZ2lmMCEwHzAH\n" \
    "BgUrDgMCGgQUj+XTGoasjY5rw8+AatRIGCx7GS4wJRYjaHR0cDovL2xvZ28udmVy\n" \
    "aXNpZ24uY29tL3ZzbG9nby5naWYwHQYDVR0OBBYEFH/TZafC3ey78DAJ80M5+gKv\n" \
    "MzEzMA0GCSqGSIb3DQEBBQUAA4IBAQCTJEowX2LP2BqYLz3q3JktvXf2pXkiOOzE\n" \
    "p6B4Eq1iDkVwZMXnl2YtmAl+X6/WzChl8gGqCBpH3vn5fJJaCGkgDdk+bW48DW7Y\n" \
    "5gaRQBi5+MHt39tBquCWIMnNZBU4gcmU7qKEKQsTb47bDN0lAtukixlE0kF6BWlK\n" \
    "WE9gyn6CagsCqiUXObXbf+eEZSqVir2G3l6BFoMtEMze/aiCKm0oHw0LxOXnGiYZ\n" \
    "4fQRbxC1lfznQgUy286dUV4otp6F01vvpX1FQHKOtw5rDgb7MzVIcbidJ4vEZV8N\n" \
    "hnacRHr2lVz2XTIIM6RUthg/aFzyQkqFOFSDX9HoLPKsEdao7WNq\n" \
    "-----END CERTIFICATE-----\n";
const char AmazonRootCA3[] PROGMEM = "-----BEGIN CERTIFICATE-----\n" \
    "MIIBtjCCAVugAwIBAgITBmyf1XSXNmY/Owua2eiedgPySjAKBggqhkjOPQQDAjA5\n" \
    "MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6b24g\n" \
    "Um9vdCBDQSAzMB4XDTE1MDUyNjAwMDAwMFoXDTQwMDUyNjAwMDAwMFowOTELMAkG\n" \
    "A1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJvb3Qg\n" \
    "Q0EgMzBZMBMGByqGSM49AgEGCCqGSM49AwEHA0IABCmXp8ZBf8ANm+gBG1bG8lKl\n" \
    "ui2yEujSLtf6ycXYqm0fc4E7O5hrOXwzpcVOho6AF2hiRVd9RFgdszflZwjrZt6j\n" \
    "QjBAMA8GA1UdEwEB/wQFMAMBAf8wDgYDVR0PAQH/BAQDAgGGMB0GA1UdDgQWBBSr\n" \
    "ttvXBp43rDCGB5Fwx5zEGbF4wDAKBggqhkjOPQQDAgNJADBGAiEA4IWSoxe3jfkr\n" \
    "BqWTrBqYaGFy+uGh0PsceGCmQ5nFuMQCIQCcAu/xlJyzlvnrxir4tiz+OpAUFteM\n" \
    "YyRIHN8wfdVoOw==\n" \
    "-----END CERTIFICATE-----\n";
const char AmazonRootCA1[] PROGMEM =  "-----BEGIN CERTIFICATE-----\n" \
    "MIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsF\n" \
    "ADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6\n" \
    "b24gUm9vdCBDQSAxMB4XDTE1MDUyNjAwMDAwMFoXDTM4MDExNzAwMDAwMFowOTEL\n" \
    "MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJv\n" \
    "b3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALJ4gHHKeNXj\n" \
    "ca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtOgQ3pOsqTQNroBvo3bSMgHFzZM\n" \
    "9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qw\n" \
    "IFAGbHrQgLKm+a/sRxmPUDgH3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6\n" \
    "VOujw5H5SNz/0egwLX0tdHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L\n" \
    "93FcXmn/6pUCyziKrlA4b9v7LWIbxcceVOF34GfID5yHI9Y/QCB/IIDEgEw+OyQm\n" \
    "jgSubJrIqg0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMC\n" \
    "AYYwHQYDVR0OBBYEFIQYzIU07LwMlJQuCFmcx7IQTgoIMA0GCSqGSIb3DQEBCwUA\n" \
    "A4IBAQCY8jdaQZChGsV2USggNiMOruYou6r4lK5IpDB/G/wkjUu0yKGX9rbxenDI\n" \
    "U5PMCCjjmCXPI6T53iHTfIUJrU6adTrCC2qJeHZERxhlbI1Bjjt/msv0tadQ1wUs\n" \
    "N+gDS63pYaACbvXy8MWy7Vu33PqUXHeeE6V/Uq2V8viTO96LXFvKWlJbYK8U90vv\n" \
    "o/ufQJVtMVT8QtPHRh8jrdkPSHCa2XV4cdFyQzR1bldZwgJcJmApzyMZFo6IQ6XU\n" \
    "5MsI+yMRQ+hDKXJioaldXgjUkK642M4UwtBV8ob2xJNDd2ZhwLnoQdeXeGADbkpy\n" \
    "rqXRfboQnoZsG4q5WTP468SQvvG5\n" \
    "-----END CERTIFICATE-----\n";
// NOTE: Don't forget to insert your certificate from the AWS IoT console:
const char clientCertPem[] PROGMEM = "-----BEGIN CERTIFICATE-----\n" \
    "...\n" \
    "-----END CERTIFICATE-----\n";
// NOTE: Don't forget to insert your private key from the AWS IoT console:
const char clientKeyPem[] PROGMEM = "-----BEGIN RSA PRIVATE KEY-----\n" \
    "...\n" \
    "-----END RSA PRIVATE KEY-----\n";

// Declarations:
enum DoorState {
  DOOR_UNKNOWN,
  DOOR_OPEN,
  DOOR_OPENING,
  DOOR_CLOSED,
  DOOR_CLOSING
};
void callback(const char* topic, byte* payload, unsigned int length);
void forceRepublishDebounced();

// State:
WiFiClientSecure espClient;
PubSubClient client("xxx-ats.iot.us-east-1.amazonaws.com", 8883, callback, espClient);
unsigned long LAST_OPEN = 0;
unsigned long LAST_CLOSED = 0;
DoorState LAST_STATE = DOOR_UNKNOWN;
const char* OPEN_TOPIC = "garageDoor/open";
const char* GET_TOPIC = "garageDoor/get";
const char* STATE_TOPIC = "garageDoor/state";
const unsigned long GET_DEBOUNCE_MS = 1000;
unsigned long LAST_GET = 0;
bool DELAY_GET = false;
bool REPUBLISH = false;

X509List trustAnchors;
X509List clientCert(clientCertPem);
PrivateKey clientKey(clientKeyPem);

// the setup function runs once when you press reset or power the board
void setup() {
  Serial.println("setup begin");
  Serial.begin(115200);

  // LED init
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  // opener relay init
  pinMode(OPENER_RELAY_PIN, OUTPUT);
  digitalWrite(OPENER_RELAY_PIN, LOW);

  // door open reed switch init
  pinMode(DOOR_OPEN_PIN, INPUT_PULLUP);

  // door closed reed switch init
  pinMode(DOOR_CLOSED_PIN, INPUT_PULLUP);

  // Configure NTP:
  configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");

  // wifi init
  Serial.println("wifi init");
  digitalWrite(LED_BUILTIN, LOW); // ON

  WiFiManager wifiManager;
  if(!wifiManager.autoConnect()) {
    Serial.println("failed to connect and hit timeout");
    ESP.reset();
  }
  Serial.println("wifi init COMPLETE");
  digitalWrite(LED_BUILTIN, HIGH); // OFF

  trustAnchors.append(VeriSign_Class_3_Public_Primary_Certification_Authority_G5);
  trustAnchors.append(AmazonRootCA3);
  trustAnchors.append(AmazonRootCA1);
  espClient.setTrustAnchors(&trustAnchors);
  espClient.setClientRSACert(&clientCert, &clientKey);
  // Set this to your AWS server:
  client.setServer("xxx-ats.iot.us-east-1.amazonaws.com", 8883);
  client.setCallback(callback);

  waitForNTP();
  Serial.println("setup complete");
}

void callback(const char* topic, byte* payload, unsigned int length) {
  if ( strcmp(topic, OPEN_TOPIC) == 0 ) {
    clickGarageDoor();
  } else if ( strcmp(topic, GET_TOPIC) == 0 ) {
    forceRepublishDebounced();
  } else {
    Serial.printf("Unknown topic: %s\n", topic);
  }
}

// Set time via NTP, as required for x.509 validation
void waitForNTP() {
  Serial.print("Waiting for NTP time sync: ");
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    digitalWrite(LED_BUILTIN, LOW); // ON
    delay(500);
    digitalWrite(LED_BUILTIN, HIGH); // OFF
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("");
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  Serial.print("Current time: ");
  Serial.print(asctime(&timeinfo));
}
void reconnect() {
    while(!client.connected()) {
        Serial.println("Connecting...");
        if(client.connect("garage-door")) {
            Serial.println("Subscribing...");
            // Resubscribe to all your topics here so that they are
            // resubscribed each time you reconnect
            client.subscribe(OPEN_TOPIC);
            client.subscribe(GET_TOPIC);
        } else {
            char BUFF[128];
            espClient.getLastSSLError(BUFF, sizeof(BUFF));
            Serial.printf("Failed to connect SSLError=%s connect=%d\n", BUFF, client.state());
            delay(5000);
        }
    }
}

void clickGarageDoor() {
   Serial.printf("clicking garage door\n");
   digitalWrite(OPENER_RELAY_PIN, HIGH);
   digitalWrite(LED_BUILTIN, LOW); // ON
   delay(1000);
   digitalWrite(OPENER_RELAY_PIN, LOW);
   digitalWrite(LED_BUILTIN, HIGH); // OFF
}

char* toString(DoorState state) {
  switch ( state ) {
  case DOOR_UNKNOWN: return "UNKNOWN";
  case DOOR_OPEN: return "OPEN";
  case DOOR_OPENING: return "OPENING";
  case DOOR_CLOSED: return "CLOSED";
  case DOOR_CLOSING: return "CLOSING";
  }
  return "INVALID";
}

DoorState getDoorState() {
  if ( digitalRead(DOOR_OPEN_PIN) == LOW ) {
    return LAST_STATE = DOOR_OPEN;
  }
  if ( digitalRead(DOOR_CLOSED_PIN) == LOW ) {
    return LAST_STATE = DOOR_CLOSED;
  }
  switch ( LAST_STATE ) {
  case DOOR_OPEN:
    LAST_OPEN = millis();
    return LAST_STATE = DOOR_CLOSING;
  case DOOR_CLOSED:
    LAST_CLOSED = millis();
    return LAST_STATE = DOOR_OPENING;
  case DOOR_OPENING:
    if ( (millis() - LAST_CLOSED) < 12000 ) {
      return LAST_STATE;
    }
    break;
  case DOOR_CLOSING:
    if ( (millis() - LAST_OPEN) < 12000 ) {
        return LAST_STATE;
    }
    break;
  }
  return LAST_STATE = DOOR_UNKNOWN;
}

bool republish(DoorState oldState, DoorState newState) {
  if ( oldState != newState ) {
    return true;
  }
  if ( REPUBLISH ) {
    return true;
  }
  unsigned long elapsed = millis() - LAST_GET;
  if ( DELAY_GET && elapsed > GET_DEBOUNCE_MS ) {
    return true;
  }
  return false;
}

void forceRepublishDebounced() {
    Serial.println("forceRepublishDebounced");

    if ( DELAY_GET ) {
      return;
    }
    unsigned long now = millis();
    if ( now - LAST_GET < GET_DEBOUNCE_MS ) {
      DELAY_GET = true;
    } else {
      LAST_GET = now;
      REPUBLISH = true;
    }
}

void loop() {
  if(!client.connected()) {
    reconnect();
    REPUBLISH = true;
  }
  client.loop();
  DoorState oldState = LAST_STATE;
  DoorState newState = getDoorState();
  if ( republish(oldState, newState) ) {
    Serial.printf("PUBLISH: %s\n", toString(newState));
    client.publish(STATE_TOPIC, toString(newState));
    REPUBLISH = false;
    DELAY_GET = false;
  }
  delay(100);
}
