#include "network.h"
#include "serial.h"
#include "config.h"
#include "pins.h"
#include <SPIFFS.h>

#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

#include <WiFi.h>

int Network::connect(String ssid, String pwd) {
  wifiConnecting = true;
  WiFi.begin(ssid, pwd);
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    Serial.print(".");
  }
  wifiConnected = true;
  Serial.print("\n");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  return 1;
}

String Network::reqTest(String host, String port) {
  WiFiClient client;
  String rs = "";

  char* empty;
  if (client.connect("10.0.0.188", strtoimax(port.c_str(), &empty, 10))) {
    Serial.println("Connect success");

    client.setNoDelay(true);
    client.println(String("POST /upload?") + "name=123&size=123&time=123\r\n");
    client.println(F("Host: 10.0.0.188"));
    client.println(F("Connection: close"));
    client.println("\r\n");

    while (client.connected() || client.available()) {
      rs=client.readString();
      // if (client.available()) {
      //   String line = client.readString();
      //   String+=line;
      // }
    }
  } else {
    Serial.println("Connect failed");
  }
  client.stop();
  return rs;
}

bool Network::isConnected() {
  return wifiConnected;
}

bool Network::isConnecting() {
  return wifiConnecting;
}

void Network::loop() {
}

Network network;
