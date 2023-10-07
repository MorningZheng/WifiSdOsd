#include "config.h"

void Config::setup() {
  data.begin("setting");
  // Serial.println(data.getString("ssid"));
  // Serial.println(data.getString("host"));
}

String Config::get(char* key,String def) {
  return data.getString(key,def);
}

void Config::set(char* key,String val) {
  data.putString(key,val);
  // Serial.print(val);
  // Serial.print("\n");
}

void Config::useDef() {
  set("ssid","private");
  set("password","xshare666888.");
  set("host","10.0.0.188");
  set("port","7788");
  set("current","");
  set("commit","");
}

Config config;
