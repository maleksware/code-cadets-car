#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "LittleFS.h"
#include <Arduino_JSON.h>
#include <Servo.h>  // Add the Servo library

#include <SPI.h>
#include <RH_RF95.h>

#define RFM95_CS 15
#define RFM95_RST 16
#define RFM95_INT 5

#define RF95_FREQ 915.0

#define LED 2

const char *ap_ssid = "Duh i kod";
const char *ap_password = "spiritcode";

// Replace with your network credentials
const char *ssid = "CC-Ext";
const char *password = "CCRasPiNetwork";

RH_RF95 rf95(RFM95_CS, RFM95_INT);


// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
// Create a WebSocket object

AsyncWebSocket ws("/ws");

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
    <title>ESP Car</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <link rel="icon" type="image/png" href="favicon.png">
    <link rel="stylesheet" type="text/css" href="style.css">
</head>
<body>
    <div class="topnav">
        <h1>ESP Car</h1>
    </div>
    <div class="content">
        <div class="card-grid">
            <div class="card">
                <p class="card-title">Steering</p>
                <p class="switch">
                    <input type="button" onclick="resetSliderPWM(this)" value="RESET">
                </p>
            </div>
            <div class="card">
                <p class="card-title">Steering</p>
                <p class="switch">
                    <input type="range" id="slider1" min="0" max="180" step="1" value="90" class="slider">
                </p>
                <p class="state">Angle: <span id="sliderValue1"></span></p>
            </div>
            <div class="card">
                <p class="card-title">Speed</p>
                <p class="switch">
                    <input type="range" id="slider2" min="50" max="115" step="1" value="90" class="slider">
                </p>
                <p class="state">Speed: <span id="sliderValue2"></span></p>
            </div>
        </div>
    </div>
    <script>
var gateway = `ws://${window.location.hostname}/ws`;
var websocket;
var lastSteeringValueSent = 90;
var lastPowerValueSent = 90;
var sendingThreshold = 10;
window.addEventListener('load', onload);
function onload(event) {
    initWebSocket();
}
function getValues(){
    websocket.send("getValues");
}
function initWebSocket() {
    console.log('Trying to open a WebSocket connection…');
    websocket = new WebSocket(gateway);
    websocket.onopen = onOpen;
    websocket.onclose = onClose;
    websocket.onmessage = onMessage;
}
function onOpen(event) {
    console.log('Connection opened');
    getValues();
}
function onClose(event) {
    console.log('Connection closed');
    setTimeout(initWebSocket, 2000);
}
function updateSliderPWM(element) {
    var sliderNumber = element.id.charAt(element.id.length-1);
    var sliderValue = document.getElementById(element.id).value;
    document.getElementById("sliderValue"+sliderNumber).innerHTML = sliderValue;
    
    if (sliderNumber == "1") {
      if (Math.abs(sliderValue - lastSteeringValueSent) >= sendingThreshold) {
        lastSteeringValueSent = sliderValue;
        websocket.send(sliderNumber+"s"+sliderValue.toString());
      }
    } else {
      if (Math.abs(sliderValue - lastPowerValueSent) >= sendingThreshold) {
        lastPowerValueSent = sliderValue;
        websocket.send(sliderNumber+"s"+sliderValue.toString());
      }
    }
}
function onMessage(event) {
    console.log(event.data);
    var myObj = JSON.parse(event.data);
    var keys = Object.keys(myObj);
    for (var i = 0; i < keys.length; i++){
        var key = keys[i];
        document.getElementById(key).innerHTML = myObj[key];
        document.getElementById("slider"+ (i+1).toString()).value = myObj[key];
    }
}

function sendSliderData() {
  updateSliderPWM(document.getElementById("slider1"));
  updateSliderPWM(document.getElementById("slider2"));
}

function resetSliderPWM() {
  document.getElementById("slider1").value = 90;
  document.getElementById("slider2").value = 90;
  updateSliderPWM(document.getElementById("slider1"));
  updateSliderPWM(document.getElementById("slider2"));
}
function setBorders(value, minval, maxval) {
  var diff = maxval - minval;
  return minval + (diff / 2) + (diff / 2) * value;
}

resetSliderPWM();
setInterval(sendSliderData, 150);
</script>
</body>
</html>
)rawliteral";

// Create Servo objects


String message = "";
String sliderValue1 = "0";
String sliderValue2 = "0";

int dutyCycle1;
int dutyCycle2;

// Json Variable to Hold Slider Values
JSONVar sliderValues;

// Get Slider Values
String getSliderValues() {
  sliderValues["sliderValue1"] = String(sliderValue1);
  sliderValues["sliderValue2"] = String(sliderValue2);

  String jsonString = JSON.stringify(sliderValues);
  return jsonString;
}

// Initialize LittleFS
void initFS() {
  if (!LittleFS.begin()) {
    Serial.println("An error has occurred while mounting LittleFS");
  } else {
    Serial.println("LittleFS mounted successfully");
  }
}

// Initialize WiFi
void initWiFi() {
  Serial.println(WiFi.softAP(ap_ssid, ap_password) ? "Ready" : "Failed!");
  
  Serial.print("Soft-AP IP address: ");
  Serial.println(WiFi.softAPIP());
  delay(1000);
}


void notifyClients(String sliderValues) {
  ws.textAll(sliderValues);
  Serial.println(sliderValues);
}


void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo *)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    message = (char *)data;
    if (message.indexOf("1s") >= 0) {  // STEERING SLIDER
      sliderValue1 = message.substring(2);
      dutyCycle1 = map(sliderValue1.toInt(), 0, 180, 0, 180);  // modify the map function to match the servo range (0-180)

      Serial.println(dutyCycle1);
      Serial.print(getSliderValues());
      notifyClients(getSliderValues());
    }
    if (message.indexOf("2s") >= 0) {  // SPEED SLIDER
      sliderValue2 = message.substring(2);
      dutyCycle2 = map(sliderValue2.toInt(), 0, 180, 0, 180);

      Serial.println(dutyCycle2);
      Serial.print(getSliderValues());
      notifyClients(getSliderValues());
    }
    if (strcmp((char *)data, "getValues") == 0) {
      notifyClients(getSliderValues());
    }
  }
}


void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
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
  Serial.println("WebSocket");
}


void setup() {
  Serial.begin(9600);
  Serial.println("successful start");

  pinMode(LED, OUTPUT);
  digitalWrite(LED,HIGH);   
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH);
  delay(100);

  pinMode(D2, INPUT);

  Serial.println();

  Serial.println("Gateway Module starting…");

  digitalWrite(RFM95_RST, LOW);
  delay(10);
  digitalWrite(RFM95_RST, HIGH);
  delay(10);

  while (!rf95.init()) {
    Serial.println("LoRa radio init failed");
    while (1);
  }

  Serial.println("LoRa radio init OK!");

  if (!rf95.setFrequency(RF95_FREQ)) {

    Serial.println("setFrequency failed");

    while (1);

  }

  Serial.print("Set Freq to: ");
  Serial.println(RF95_FREQ);
  rf95.setTxPower(23, false);

  initFS();
  initWiFi();
  initWebSocket();

  // Web Server Root URL
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", index_html);
  });

  server.serveStatic("/", LittleFS, "/");

  // Start server
  server.begin();
}

int prevEngPower = 90;

String dec2hex(int val) {
  char alphabet[17] = "0123456789ABCDEF";
  String res;

  res += alphabet[val / 16];
  res += alphabet[val % 16];

  return res;
}


void loop() {
  // steering.write(dutyCycle1); // dutyCycle1 contains the first slider value already converted to degrees

  Serial.println("Sending to car №96");
  // Send a message to rf95_server

  char radiopacket[32] = "96|      ";
  
  // a bad way to write stuff into the packet, but should work

  Serial.print("Cycle values: ");
  Serial.print(dutyCycle1);
  Serial.print(" ");
  Serial.println(dutyCycle2);

  String steeringValueHex = dec2hex(dutyCycle1);
  radiopacket[3] = steeringValueHex[0];
  radiopacket[4] = steeringValueHex[1];

  String powerValueHex = dec2hex(dutyCycle2);
  radiopacket[5] = powerValueHex[0];
  radiopacket[6] = powerValueHex[1];

  if (digitalRead(D2)) {
    radiopacket[7] = '1';
  } else {
    radiopacket[7] = '0';
  }

  radiopacket[8] = dec2hex((dutyCycle1 + dutyCycle2) % 16)[1];

  Serial.print("Sending ");
  Serial.println(radiopacket);
  radiopacket[31] = 0;

  Serial.println("Sending...");
  delay(10);
  rf95.send((uint8_t *)radiopacket, 20);

  digitalWrite(LED, HIGH);

  Serial.println("Waiting for packet to complete...");
  delay(10);
  Serial.println("delay");
  rf95.waitPacketSent();
  Serial.println("unfreeze");
  // Now wait for a reply
  uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
  uint8_t len = sizeof(buf);

  Serial.println("Waiting for reply...");
  delay(10);

  if (rf95.waitAvailableTimeout(1000)) {
    // Should be a reply message for us now
    if (rf95.recv(buf, &len)) {
      if (buf[0] == '9' && buf[1] == '6' && buf[2] == '|') {
        Serial.print("Got reply: ");
        Serial.println((char *)buf);
        Serial.print("RSSI: ");
        Serial.println(rf95.lastRssi(), DEC);
      }
    } else {
      Serial.println("Receive failed");
    }
  } else {
    Serial.println("No reply, is there a listener around?");
  }

  delay(100);
  digitalWrite(LED, LOW);
}