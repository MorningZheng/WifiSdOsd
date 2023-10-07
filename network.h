#ifndef _NETWORK_H_
#define _NETWORK_H_

#if defined(ARDUINO) && ARDUINO >= 100
    #include "Arduino.h"
#else
    #include "WProgram.h"
#endif

class Network {
public:
  Network() { wifiConnecting = true;}
  bool isConnected();
  bool isConnecting();
  int connect(String pssid,String ppass);
  void loop();
  String reqTest(String host,String port);

private:
  bool wifiConnected;
  bool wifiConnecting;
};

extern Network network;

#endif
