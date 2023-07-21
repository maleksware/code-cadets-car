#include <SPI.h>
#include <RH_RF95.h>
#include <Servo.h>
#include <stdlib.h>

#define RFM95_CS 15
#define RFM95_RST 16
#define RFM95_INT 5

#define RF95_FREQ 915.0

#define LED 2

RH_RF95 rf95(RFM95_CS, RFM95_INT);

//Servos
Servo steering;
Servo ESC;


void setup() 
{
  pinMode(LED, OUTPUT);
  digitalWrite(LED,HIGH);
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH);
  Serial.begin(9600);
  delay(100);
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
     // The default transmitter power is 13dBm, using PA_BOOST.
    // If you are using RFM95 / 96/97/98 modules using the transmitter pin PA_BOOST, then
    // you can set transmission powers from 5 to 23 dBm:
  rf95.setTxPower(23, false);
  steering.attach(D3);
  ESC.attach(D2);
}

// Input HexValue, return DecValue
int hexadecimalToDecimal(String hexVal) {
  int len = 2;
  int base = 1;
  int dec_val = 0;
  for (int i = len - 1; i >= 0; i--) {
    if (hexVal[i] >= '0' && hexVal[i] <= '9') {
        dec_val += (int(hexVal[i]) - 48) * base;
        base = base * 16;
    }
    else if (hexVal[i] >= 'A' && hexVal[i] <= 'F') {
        dec_val += (int(hexVal[i]) - 55) * base;
        base = base * 16;
    }
  }
  return dec_val;
}


void loop() {
  if (rf95.available())
  {
    // Should be a message for us now   
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);
    if (rf95.recv(buf, &len))
    {
      digitalWrite(LED, HIGH);
      // 0 = 9, 1 = 6, 2 = |, 3-4 = Hex(steering), 5-6 = Hex(power) 7 = Emergency Stop, 8 = Checksum
      if (buf[0] == '9' && buf[1] == '6' && buf[2] == '|') {
        Serial.println("got a message!");
        if (buf[7] != '0'){
          //Emergency Stop
          Serial.println("EMERGENCY STOP");
          steering.write(90);
          ESC.write(90);
          abort();
        } else {
          char hexSteering[2];
          char hexPower[2];
          //Write out incoming command to serial monitor
          Serial.print("Incoming command: ");
          Serial.println((char *)buf);
          //Get the hex value of steering
          hexSteering[0] = buf[3];
          hexSteering[1] = buf[4];
          //Get the hex value of power for ESC
          hexPower[0] = buf[5];
          hexPower[1] = buf[6];

          char hexChecksum[2];
          hexChecksum[1] = buf[8];
          hexChecksum[0] = '0';

          Serial.println(hexadecimalToDecimal(hexSteering));
          Serial.println(hexadecimalToDecimal(hexPower));
          
          //Write steering decimal value, after converting from hex
          steering.write(hexadecimalToDecimal(hexSteering));
          //Set power variable to the decimal return value of hex value
          int steeringValue = hexadecimalToDecimal(hexSteering);
          int powerValue = hexadecimalToDecimal(hexPower);

          int checksum = hexadecimalToDecimal(hexChecksum);
          Serial.println(checksum);
          bool invalidCheckSum = (checksum != (steeringValue + powerValue) % 16);
          Serial.println((steeringValue + powerValue) % 16);

          if (invalidCheckSum) {
            Serial.println("BROKEN PACKET!!!");
          } else {
            steering.write(steeringValue);
            Serial.println("power written");
            //Write power to ESC
            if (powerValue < 90) {
              //If reversing, set to 90 (still) first
              ESC.write(90);
              delay(10);
              ESC.write(powerValue);
            } else {
              //Accelerate
              ESC.write(powerValue);
            }
          }
          // Send a reply
          uint8_t data[] = "96|RECIEVED|";
          rf95.send(data, sizeof(data));
          rf95.waitPacketSent();
          Serial.println("Sent a reply");
          digitalWrite(LED, LOW);
          
        }
      }
    }
    else
    {
      Serial.println("Receive failed");
    }
  }
}