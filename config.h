#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <stdio.h>
#include <string.h>
#include <Preferences.h>

class Config {
  public:
    Config() {}
    void setup();
    String get(char* key,String def="");
    void set(char* key, String val);
    void useDef();

  protected:
    Preferences data;
};

extern Config config;

#endif
