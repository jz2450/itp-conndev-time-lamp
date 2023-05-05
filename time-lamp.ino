// TODO
// button debouncing
// intl date line handling

#include <WiFiNINA.h>  // use this for Nano 33 IoT, MKR1010, Uno WiFi
#include <Wire.h>
#include <ArduinoHttpClient.h>
#include "arduino_secrets.h"
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <TimeLib.h>

char serverAddress[] = "api.sunrise-sunset.org";  // server address

// need a button for modes

// uncomment to set to brooklyn
// const float LATITUDE = 40.69289993261725;
// const float LONGITUDE = -73.98744577126318;

// uncomment to set to auckland
const float LATITUDE = -36.88075221967886;
const float LONGITUDE = 174.79819137197455;


const int bulbPin = 17;
const int ledPin = 16;
const int buttonPin = 21;

int buttonState = 0;
int buttonHistory = 0;
int modeState = 0;
// 0 = manual off
// 1 = manual on
// 2 = time zone mode
int prevState = 0;
unsigned long startMillis = 0;
int brightness = 50;

// initialize WiFi connection as SSL:
// WiFiSSLClient wifi;
WiFiClient wifi;
HttpClient httpClient = HttpClient(wifi, serverAddress);

// for timestamping
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

String macAddr;

void setup() {
  // initialize serial:
  Serial.begin(9600);
  // wait for serial monitor to open:
  if (!Serial) delay(3000);
  // set pins
  pinMode(ledPin, OUTPUT);
  pinMode(bulbPin, OUTPUT);
  pinMode(buttonPin, INPUT);
  // get MAC address:
  byte mac[6];
  WiFi.macAddress(mac);
  // put it in a string:
  for (int i = 5; i >= 0; i--) {
    if (mac[i] < 16) macAddr += "0";
    macAddr += String(mac[i], HEX);
  }
  Serial.println();
}

void loop() {
  if (modeState == 0) {
    // manual off mode
    Serial.println("off mode");
    analogWrite(bulbPin, 0);
    digitalWrite(ledPin, LOW);
  } else if (modeState == 1) {
    // manual on mode
    Serial.println("on mode");
    analogWrite(bulbPin, 255);
    digitalWrite(ledPin, HIGH);
  } else {
    // time zone mode
    // Serial.println("time zone mode");
    // digitalWrite(ledPin, HIGH);
    if ((millis() - startMillis) % 60000 == 0 || buttonState != buttonHistory) {
      //if you disconnected from the network, reconnect:
      if (WiFi.status() != WL_CONNECTED) {
        digitalWrite(ledPin, LOW);
        analogWrite(bulbPin, 128);
        connectToNetwork();
        // skip the rest of the loop until you are connected:
        return;
      } else {
        timeClient.begin();
      }
      timeClient.update();
      digitalWrite(ledPin, LOW);
      makeGetRequest();
      digitalWrite(ledPin, HIGH);
      // Serial.println("waiting 60 seconds until next API call");
      if (buttonState != buttonHistory) {
        startMillis = millis();
      }
      Serial.println(String(startMillis));
    }
    timeClient.update();
  }

  // button logic
  buttonHistory = buttonState;
  buttonState = digitalRead(buttonPin);
  if (buttonState == HIGH && buttonState != buttonHistory) {
    prevState == modeState;
    modeState++;
    if (modeState > 2) modeState = 0;
  }
}

void connectToNetwork() {
  // try to connect to the network:
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("Attempting to connect to: " + String(SECRET_SSID));
    //Connect to WPA / WPA2 network:
    WiFi.begin(SECRET_SSID, SECRET_PASS);
    delay(2000);
  }
  // print IP address once connected:
  Serial.print("Connected. My IP address: ");
  Serial.println(WiFi.localIP());
}

void makeGetRequest() {
  Serial.println("making GET request");
  // send query with coordinates
  String request = "/json?lat=LATITUDE&lng=LONGITUDE&formatted=0";
  request.replace("LATITUDE", String(LATITUDE));
  request.replace("LONGITUDE", String(LONGITUDE));
  Serial.println("getting " + request);
  httpClient.get(request);

  // read the status code and body of the response
  int statusCode = httpClient.responseStatusCode();
  String response = httpClient.responseBody();

  Serial.print("Status code: ");
  Serial.println(statusCode);
  Serial.print("Response: ");
  Serial.println(response);

  parseResponse(response);
}

void makeTmmrGetRequest() {
  Serial.println("making GET request for TOMORROW");
  // send query with coordinates
  String request = "/json?lat=LATITUDE&lng=LONGITUDE&date=tomorrow&formatted=0";
  request.replace("LATITUDE", String(LATITUDE));
  request.replace("LONGITUDE", String(LONGITUDE));
  Serial.println("getting " + request);
  httpClient.get(request);

  // read the status code and body of the response
  int statusCode = httpClient.responseStatusCode();
  String response = httpClient.responseBody();

  Serial.print("Status code: ");
  Serial.println(statusCode);
  Serial.print("Response: ");
  Serial.println(response);

  parseResponse(response);
}

void parseResponse(String response) {
  // parse in time to epoch UTC
  // FORMAT
  // "sunrise":"2023-04-11T10:21:53+00:00","sunset":"2023-04-11T23:32:06+00:00"
  int sunriseIndex = response.indexOf("sunrise");
  int sunsetIndex = response.indexOf("sunset");
  int endIndex = response.indexOf("solar");
  String sunriseString = response.substring(sunriseIndex, sunsetIndex);
  String sunsetString = response.substring(sunsetIndex, endIndex);
  // Serial.println(sunriseString);

  time_t sunriseEpoch = tmConvert_t(sunriseString.substring(10, 14).toInt(), sunriseString.substring(15, 17).toInt(), sunriseString.substring(18, 20).toInt(), sunriseString.substring(21, 23).toInt(), sunriseString.substring(24, 26).toInt(), sunriseString.substring(27, 29).toInt());
  time_t sunsetEpoch = tmConvert_t(sunsetString.substring(9, 13).toInt(), sunsetString.substring(14, 16).toInt(), sunsetString.substring(17, 19).toInt(), sunsetString.substring(20, 22).toInt(), sunsetString.substring(23, 25).toInt(), sunsetString.substring(26, 28).toInt());

  // Serial.println(sunriseEpoch);
  // Serial.println(sunsetEpoch);
  int secondsSinceSunrise = timeClient.getEpochTime() - sunriseEpoch;
  int dayLenSeconds = sunsetEpoch - sunriseEpoch;
  int middayEpoch = sunriseEpoch + (dayLenSeconds / 2);
  // Serial.println(dayLenSeconds);  // check it's the same as the API response
  // Serial.println(secondsSinceSunrise);

  Serial.println("current time: " + String(timeClient.getEpochTime()));
  Serial.println("sunrise time: " + String(int(sunriseEpoch)));
  Serial.println("sunset time: " + String(int(sunsetEpoch)));

  if (timeClient.getEpochTime() < middayEpoch && timeClient.getEpochTime() > sunriseEpoch) {
    brightness = map(timeClient.getEpochTime(), sunriseEpoch, middayEpoch, 0, 255);
  } else if (timeClient.getEpochTime() >= middayEpoch && timeClient.getEpochTime() < sunsetEpoch) {
    brightness = map(timeClient.getEpochTime(), middayEpoch, sunsetEpoch, 255, 0);
  } else if (timeClient.getEpochTime() > sunsetEpoch) {
    //check if it's the next day in nz
    makeTmmrGetRequest();
  } else {
    brightness = 0;
  }

  Serial.println("setting brightness to " + String(brightness));
  analogWrite(bulbPin, brightness);
  // analogWrite(ledPin, brightness);
}

// https://forum.arduino.cc/t/convert-time-to-epoch/379909/2
time_t tmConvert_t(int YYYY, byte MM, byte DD, byte hh, byte mm, byte ss) {
  tmElements_t tmSet;
  tmSet.Year = YYYY - 1970;
  tmSet.Month = MM;
  tmSet.Day = DD;
  tmSet.Hour = hh;
  tmSet.Minute = mm;
  tmSet.Second = ss;
  return makeTime(tmSet);
}