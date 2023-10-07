#include <Arduino.h>
#include "serial.h"
#include <SPIFFS.h>
#include "config.h"
#include "sdControl.h"
#include <SD.h>
#include "md5.h"
#include <WiFi.h>
#include "network.h"
#include <UrlEncode.h>
#include <HTTPClient.h>

String current;
const String EMPTY_STRING = "";
String HOST;
int PORT;


#define FREEZE_TIME 90 * 1000
#define BOOT_WAIT_TIME 15 * 1000
#define NEXT_WAIT_TIME 60 * 1000

#define MTU_SIZE 4096  // this size seems to work best
// file sending from SD card
byte clientBuf[MTU_SIZE];
TaskHandle_t myInit;

String md5(char* text) {
    unsigned char* make = MD5::make_hash(text);
    const char* hash = MD5::make_digest(make, 16);
    // Serial.println(md5str);
    free(make);
    make = NULL;
    return hash;
}

void setup() {
    SERIAL_INIT(115200);
    setCpuFrequencyMhz(240);//提高运行频率
    config.setup();

    HOST = config.get("host");
    PORT = config.get("port", "80").toInt();

    Serial.println("Booting...");

    // 启动多任务，看看是否能解决锁死的情况
    xTaskCreatePinnedToCore(doInit, "init", 16 * 1024, NULL, 1, &myInit, tskNO_AFFINITY);
    Serial.println("xTaskCreatePinnedToCore");


    // sdControl.takeControl();
    // doUpload("/abc.txt");
    // sdControl.relinquishControl();

    // config.useDef();
}

void loop() {}

void doInit(void* pvParameters) {
    (void)pvParameters;
    Serial.println("Initing...");

    //链接wifi
    delay(BOOT_WAIT_TIME);
    Serial.println("connect to wifi: " + config.get("ssid") + " " + config.get("password"));
    network.connect(config.get("ssid"), config.get("password"));

    // 告诉服务器我上线了
    HTTPClient http;
    const String ip = WiFi.localIP().toString();
    const String mac = String(WiFi.macAddress());
    http.begin(("http://" + HOST + ":" + String(PORT) + "/online?mac=" + urlEncode(mac) + "&ip=" + urlEncode(ip)));
    int resCode = http.GET();
    if (resCode > 0) {
        String payload = http.getString();
        Serial.println(payload);
    } else {
        Serial.print("Error code: ");
        Serial.println(resCode);
    }
    // Free resources
    http.end();

    delay(BOOT_WAIT_TIME);
    // 初始化读卡器
    SPIFFS.begin();
    sdControl.setup();

    delay(BOOT_WAIT_TIME);
    Serial.println("Check for SD change.");
    readChange();
}

void readChange() {
    sdControl.takeControl();
    current = "";

    if (!SD.exists("/.upload")) {
        SD.mkdir("/.upload");
    }
    readChangeWorker("/");
    sdControl.relinquishControl();

    //文件没有变化
    if (strlen(current.c_str()) < 10) {
        // readChange();
        Serial.println("File no change,Waiting for next check.");
        delay(NEXT_WAIT_TIME);
        readChange();  //下一次确认变化循环
        return;
    }

    const String hash = md5((char*)current.c_str());

    //文件变化已经上传
    if (config.get("commit") == hash) {
        Serial.println("File have uploaded,Waiting for next check.");
        delay(NEXT_WAIT_TIME);
        readChange();  //下一次确认变化循环
        return;
    }

    //冷却过后
    if (config.get("current") != hash) {
        // readChange();
        Serial.println("File change,Waiting for freezing.");
        config.set("current", hash);
        Serial.println("");
        delay(FREEZE_TIME);
        readChange();  //再次确认变化情况
        return;
    }

    //不再变化，执行阻塞上传
    Serial.println(current);
    Serial.println("Uploading");
    startUpload();
    Serial.println("Set commit to " + hash);
    delay(NEXT_WAIT_TIME);
    readChange();  //下一次确认变化循环
}

void readChangeWorker(String path) {
    File file = SD.open(path, FILE_READ);
    if (file.isDirectory()) {
        file.rewindDirectory();
        while (true) {
            File entry = file.openNextFile();
            if (!entry) break;

            const String name = entry.name();
            entry.close();

            if (0 == (strcmp(name.c_str(), ".upload")) || 0 == (strcmp(name.c_str(), "System Volume Information"))) {
                continue;
            }

            String temp;
            if (path == "/") temp = "";
            else temp = path;
            temp += "/" + String(name);

            readChangeWorker(temp);
        };
        file.close();
    } else {
        file.close();

        const String hash = md5((char*)path.c_str());
        const String uploaded = "/.upload/" + hash;

        // Serial.println(path);
        // Serial.println(uploaded);
        //文件已将上传，不再参与计算
        if (SD.exists(uploaded)) return;
        // Serial.println(uploaded);
        // Serial.println(String(path));

        // //生成路径变化值
        current += String(path) + "\n";
    }
}

void startUpload() {
    sdControl.takeControl();
    int pos = 0;
    int rs = true;
    while (true) {
        const int end = current.indexOf("\n", pos);
        if (end == -1) {
            if (!doUpload(current.substring(pos, end))) {
                rs = false;
                break;
            };
            break;
        } else {
            if (!doUpload(current.substring(pos, end))) {
                rs = false;
                break;
            };
            pos = end + 1;
        }
    }
    sdControl.relinquishControl();
}

bool doUpload(String path) {
    path.trim();
    if (!path.length()) return true;

    WiFiClient client;
    if (client.connect(HOST.c_str(), PORT)) {
        Serial.println("Uploading " + path);

        File dataFile = SD.open(path, FILE_READ);
        const String name = dataFile.name();
        const String size = String(dataFile.size());

        client.setNoDelay(true);
        const String req = String("POST ") + "/upload?name=" + urlEncode(name) + "&size=" + size + " HTTP/1.1\r\n"
                           + "Host: " + HOST + "\r\n"
                           + "Content-Type: application/octet-stream\r\n"
                           + "User-Agent: Arduino/auto-upload\r\n"
                           + "Keep-Alive: 300\r\n"
                           + "Connection: keep-alive\r\n"
                           + "Content-Length: " + size + "\r\n\r\n";
        char buff[req.length() + 1];
        req.toCharArray(buff, req.length() + 1);
        client.write(buff);

        int clientCount = 0;
        while (dataFile.available()) {
            if (dataFile.available() < MTU_SIZE) {
                clientCount = dataFile.available();
            } else {
                clientCount = MTU_SIZE;
            }

            if (clientCount > 0) {
                dataFile.read(&clientBuf[0], clientCount);
                client.write((const uint8_t*)&clientBuf[0], clientCount);
                clientCount = 0;
            }
        }
        // Serial.println("File end.");
        dataFile.close();

        String res = client.readString();
        client.stop();
        res.trim();
        const String txt = res.substring(res.indexOf("\r\n\r\n", 16) + 4);
        const String hash = md5((char*)path.c_str());
        const String uploaded = "/.upload/" + hash;
        Serial.println("Response data:" + txt);

        if (txt == size) {
            Serial.println("Write flag as " + uploaded + ",size:" + size);
            File file = SD.open(uploaded, FILE_WRITE);
            file.print(size);
            file.close();
        } else {
            Serial.println("Upload failed");
        }
        return true;
    } else {
        Serial.println("Connect failed");
        return false;
    }
}
