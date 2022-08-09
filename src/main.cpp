#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <NewPing.h>
WiFiServer server(80);
WiFiServer server2(79);
String header;

int currentGarageDoorState;

int targetGarageDoorState = -1;

const int trig = 12;
const int echo = 14;
const int max_distance = 400;

NewPing sonar(trig, echo, max_distance);

float duration, distance;

const int iterations = 5;

unsigned long currentTime = millis();
// Previous time
unsigned long previousTime = 0;
// Define timeout time in milliseconds (example: 2000ms = 2s)
const long timeoutTime = 2000;
void setup()
{
  Serial.begin(9600);
  WiFiManager wifiManager;
  // wifiManager.resetSettings();
  wifiManager.setSTAStaticIPConfig(IPAddress(10, 0, 0, 102), IPAddress(10, 0, 0, 1), IPAddress(255, 255, 255, 0)); // optional DNS 4th argument
  wifiManager.autoConnect("Detector Bridge");
  Serial.println("Connected!");
  server.begin();
}

void isDoorThere()
{
  duration = sonar.ping_median(iterations);
  distance = duration / 2 * 0.0343;
  if (distance > 15)
  {
    currentGarageDoorState = 0;
  }
  if (distance < 15)
  {
    currentGarageDoorState = 1;
  }
  if (distance < 0.02)
  {
    currentGarageDoorState = 0;
  }
}

void getTargetDoorState(WiFiClient client)
{
  HTTPClient http;
  http.begin(client, "http://10.0.0.101:80/status");
  StaticJsonDocument<96> doc;
  http.GET();
  DeserializationError error = deserializeJson(doc, http.getString());

  // int currentDoorState = doc["currentDoorState"]; // 0
  targetGarageDoorState = doc["targetDoorState"]; // 1
  http.end();
}

// void checkForDisruption(WiFiClient client)
// {
//   // Serial.println("Checking for disruption...");
//   // Serial.print(currentGarageDoorState);
//   // Serial.print(" ");
//   // Serial.print(targetGarageDoorState);
//   if (currentGarageDoorState != targetGarageDoorState || targetGarageDoorState == -1)
//   {

//     // Serial.println("Disruption detected");
//     isDoorThere();
//     targetGarageDoorState = currentGarageDoorState;

//   }
// }

void loop()
{
  WiFiClient client = server.available();   // Listen for incoming clients
  WiFiClient client2 = server2.available(); // Listen for incoming clients
  HTTPClient http;

  // This block of code before if(client) is the watchdog service for a garage update that is not handled by a smart home, i.e cars, remotes.

  isDoorThere();

  // Serial.println(currentGarageDoorState);
  // Serial.println(distance);
  if (client)
  { // If a new client connects,
    // Serial.println("New Client."); // print a message out in the serial port
    String currentLine = ""; // make a String to hold incoming data from the client
    while (client.connected())
    { // loop while the client's connected
      if (client.available())
      {                         // if there's bytes to read from the client,
        char c = client.read(); // read a byte, then
        // Serial.write(c);        // print it out the serial monitor
        header += c;
        if (c == '\n')
        { // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0)
          {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();

            // turns the GPIOs on and off
            if (header.indexOf("GET /status") >= 0)
            {
              getTargetDoorState(client2);

              // print the distance to serial monitor
              // Serial.print("Distance: ");
              // Serial.println(distance);
              StaticJsonDocument<32> doc;

              doc["currentDoorState"] = currentGarageDoorState;
              doc["targetDoorState"] = targetGarageDoorState;
              String output;
              serializeJson(doc, output);
              client.println(output);
              // client.println(targetGarageDoorState); // 0 for open, 1 for closed, 2 for opening, and 3 for closing
            }
            break;
          }
          else
          { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        }
        else if (c != '\r')
        {                   // if you got anything else but a carriage return character,
          currentLine += c; // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    // Serial.println("Client disconnected.");
    // Serial.println("");
  }
}
