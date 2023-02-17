#include <WiFi.h>
#include <ArduinoJson.h>
#include <Adafruit_Fingerprint.h>

#define BAUD_RATE 9600
#define USERID "admin" // 변경 가능

String sha_result = "";

char ssid[] = "SK_WiFiGIGAFA30";
char pass[] = "1907021960";
int status = WL_IDLE_STATUS;

unsigned long lastConnectionTime = 0;
const unsigned long postingInterval = 5000L;

String rcvbuf;
boolean getIsConnected = false;

//연결할 서버 정보
IPAddress hostIp(172,30,1,58);
int SERVER_PORT = 8000;

WiFiClient client;

//서버로 부터 받는 정보들
String userid;
int fid;

//지문 인식 관련 변수
#if (defined(__AVR__) || defined(ESP8266)) && !defined(__AVR_ATmega2560__)
// For UNO and others without hardware serial, we must use software serial...
// pin #2 is IN from sensor (GREEN wire)
// pin #3 is OUT from arduino  (WHITE wire)
// Set up the serial port to use softwareserial..
SoftwareSerial mySerial(2, 3);

#else
// On Leonardo/M0/etc, others with hardware serial, use hardware serial!
// #0 is green wire, #1 is white
#define mySerial Serial1

#endif

Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

uint8_t id;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(BAUD_RATE);
  // 네트워크 연결부
  Serial.println("Attempting to connect to WPA network...");
  status = WiFi.begin(ssid, pass);

  if(WiFi.status() == WL_NO_SHIELD){
    Serial.println("WiFi shield not present");
    while(true);
  }
  else{
    Serial.println("WiFi shield is load...");
  }

  Serial.print(F("Firmware version: "));
  Serial.println(WiFi.firmwareVersion());

  if (status != WL_CONNECTED){
    Serial.println("Couldn't get a wifi connection");
    while(true);
  }

  else{
    Serial.println("Connected to network");

    Serial.println("------------<network info>-----------");
    printWifiData();
    printCurrentNet();
    Serial.println("-------------------------------------");
  }
  // 지문 인식 세팅
  finger.begin(57600);
  if (finger.verifyPassword()) {
    Serial.println("Found fingerprint sensor!");
  } else {
    Serial.println("Did not find fingerprint sensor :(");
    while (1) { delay(1); }
  }

  Serial.println(F("Reading sensor parameters"));
  finger.getParameters();
  Serial.print(F("Status: 0x")); Serial.println(finger.status_reg, HEX);
  Serial.print(F("Sys ID: 0x")); Serial.println(finger.system_id, HEX);
  Serial.print(F("Capacity: ")); Serial.println(finger.capacity);
  Serial.print(F("Security level: ")); Serial.println(finger.security_level);
  Serial.print(F("Device address: ")); Serial.println(finger.device_addr, HEX);
  Serial.print(F("Packet len: ")); Serial.println(finger.packet_len);
  Serial.print(F("Baud rate: ")); Serial.println(finger.baud_rate);
}

uint8_t readnumber(void) {
  uint8_t num = 0;

  while (num == 0) {
    while (! Serial.available());
    num = Serial.parseInt();
  }
  return num;
}

void loop() {
  // put your main code here, to run repeatedly:
  fid = -1;
  int choice = selectMenu();
  switch(choice){
    case 1:
      fpRegister();      
      break;
    case 2:
      fpCheck();
      break;
  }
}

int selectMenu(){
  Serial.println("Select Menu (1.fingerprint Registeration // 2.fingerprint Login) >>");
  return Serial.parseInt();
}

void fpRegister(){
  // 로그인
  Serial.print("ID: ");
  String username = Serial.readStringUntil('\n');
  Serial.print("PASSWORd: ");
  String password = Serial.readStringUntil('\n');
  postAccountToServer(username, password);
  // 로그인 실패 시(값 없음:128 // 아이디 없음:-1)
  if(fid = -1){
    return;
  }

  // 지문 등록
  if (fid == 128){
    Serial.println("Ready to enroll a fingerprint!");
    Serial.println("Please type in the ID # (from 1 to 127) you want to save this finger as...");
    id = readnumber();
    if (id < 1 || id > 127) {// ID #0 not allowed, try again!
      return;
    }
    Serial.print("Enrolling ID #");
    Serial.println(id);
    fid = id;
  }
  //fid값 등록 지문으로 설정 될 것
  while (getFingerprintEnroll() != true );
  
  //서버 전송부
  postFidToServer(fid, username);
}

void fpCheck(){

}

void postAccountToServer(String username, String password){
  if (client.connect(hostIp, SERVER_PORT)){
    Serial.println("Connecting...");

    String jsondata = "";

    StaticJsonDocument<200> root;
    // JsonObject& root = jsonBuffer.createObject();/
    root["username"] = username;
    root["password"] = password;
    
    userid = username;    

    serializeJson(root, jsondata);
    Serial.println(jsondata);

     // send the HTTP POST request
    client.print(F("POST /fpregister"));
    client.print(F(" HTTP/1.1\r\n"));
    client.print(F("Cache-Control: no-cache\r\n"));
    client.print(F("Host: 172.30.1.58:8000\r\n")); //IP세팅하고
    client.print(F("User-Agent: Arduino\r\n"));
    client.print(F("Content-Type: application/json;charset=UTF-8\r\n"));
    client.print(F("Content-Length: "));
    client.println(jsondata.length());
    client.println();
    client.println(jsondata);
    client.print(F("\r\n\r\n"));

    lastConnectionTime = millis();
    getIsConnected = true;
  }
  else {
    Serial.println("connection failed");
    getIsConnected = false;
  }
  //값 받는 부분
  int headcount = 0;
 
  //No Socket available문제 해결//
  while (client.connected()) {
    if (client.available() && status == WL_CONNECTED) {
      char c = client.read();

      //String에 담아서 원하는 부분만 파싱하도록 함//
      rcvbuf += c;
      
      if(c == '\r'){
        headcount ++; //해더 정보는 생략하기 위해서 설정//
    
        if(headcount != 13){
          rcvbuf = "";
        }
      }

      //데이터 영역/
      if(headcount == 13){
        //JSON파싱//
        StaticJsonDocument<200> root;
        // JsonObject& root = jsonBuffer.parseOb/ject(rcvbuf);
        DeserializationError error = deserializeJson(root, rcvbuf);
        if (error)
          return;
        fid = root["fid"];
        
        Serial.println(fid);
  
        client.stop(); //클라이언트 접속 해제//
        
        rcvbuf = "";
      }
    }
  }

  client.flush();
  client.stop();
}

void postFidToServer(int fid, String username){
  if (client.connect(hostIp, SERVER_PORT)){
    Serial.println("Connecting...");

    String jsondata = "";

    StaticJsonDocument<200> root;
    // JsonObject& root = jsonBuffe/r.createObject();
    root["username"] = username;
    root["fid"] = fid;
    
    // root.printTo(jsondata);
    serializeJson(root, jsondata);
    Serial.println(jsondata);

     // send the HTTP POST request
    client.print(F("POST /lightdata"));
    client.print(F(" HTTP/1.1\r\n"));
    client.print(F("Cache-Control: no-cache\r\n"));
    client.print(F("Host: 172.30.1.58:8000\r\n")); //IP세팅하고
    client.print(F("User-Agent: Arduino\r\n"));
    client.print(F("Content-Type: application/json;charset=UTF-8\r\n"));
    client.print(F("Content-Length: "));
    client.println(jsondata.length());
    client.println();
    client.println(jsondata);
    client.print(F("\r\n\r\n"));

    lastConnectionTime = millis();
    getIsConnected = true;
  }
  else {
    Serial.println("connection failed");
    getIsConnected = false;
  }

  //값 받는 부분
  // int headcount = 0;
 
  // //No Socket available문제 해결//
  // while (client.connected()) {
  //   if (client.available() && status == WL_CONNECTED) {
  //     char c = client.read();

  //     //String에 담아서 원하는 부분만 파싱하도록 함//
  //     rcvbuf += c;
      
  //     if(c == '\r'){
  //       headcount ++; //해더 정보는 생략하기 위해서 설정//
    
  //       if(headcount != 13){
  //         rcvbuf = "";
  //       }
  //     }

  //     //데이터 영역/
  //     if(headcount == 13){
  //       //JSON파싱//
  //       StaticJsonDocument<200> root;
  //         // JsonObject& root = jsonBuffer.parseOb/ject(rcvbuf);
  //       DeserializationError error = deserializeJson(root, rcvbuf);
          //  if (error)
          //    return;
          //  String result = root["result"];
           
  //       Serial.println(result);
  
  //       client.stop(); //클라이언트 접속 해제//
        
  //       rcvbuf = "";
  //     }
  //   }
  // }

  // client.flush();
  // client.stop();
}


void printWifiData() {
  // WI-FI 실드의 IP를 출력한다.
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);
  Serial.println(ip);
  
  // MAC어드레스를 출력한다.
  byte mac[6];  
  WiFi.macAddress(mac);
  Serial.print("MAC address: ");
  Serial.print(mac[5],HEX);
  Serial.print(":");
  Serial.print(mac[4],HEX);
  Serial.print(":");
  Serial.print(mac[3],HEX);
  Serial.print(":");
  Serial.print(mac[2],HEX);
  Serial.print(":");
  Serial.print(mac[1],HEX);
  Serial.print(":");
  Serial.println(mac[0],HEX);
}

void printCurrentNet() {
  // 접속하려는 네트워크의 SSID를 출력한다.
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // 접속하려는 router의 MAC 주소를 출력한다.
  byte bssid[6];
  WiFi.BSSID(bssid);    
  Serial.print("BSSID: ");
  Serial.print(bssid[5],HEX);
  Serial.print(":");
  Serial.print(bssid[4],HEX);
  Serial.print(":");
  Serial.print(bssid[3],HEX);
  Serial.print(":");
  Serial.print(bssid[2],HEX);
  Serial.print(":");
  Serial.print(bssid[1],HEX);
  Serial.print(":");
  Serial.println(bssid[0],HEX);

  // 수신 신호 강도를 출력한다.
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.println(rssi);

  // 암호화 타입을 출력한다.
  byte encryption = WiFi.encryptionType();
  Serial.print("Encryption Type:");
  Serial.println(encryption,HEX);
  Serial.println();
}

int getFingerprintEnroll() {
  int p = -1;
  Serial.print("Waiting for valid finger to enroll as #"); Serial.println(fid);
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      Serial.println(".");
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      break;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");
      break;
    default:
      Serial.println("Unknown error");
      break;
    }
  }

  // OK success!

  p = finger.image2Tz(1);
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  Serial.println("Remove finger");
  delay(2000);
  p = 0;
  while (p != FINGERPRINT_NOFINGER) {
    p = finger.getImage();
  }
  Serial.print("ID "); Serial.println(fid);
  p = -1;
  Serial.println("Place same finger again");
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      Serial.print(".");
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      break;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");
      break;
    default:
      Serial.println("Unknown error");
      break;
    }
  }

  // OK success!
  p = finger.image2Tz(2);
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  // OK converted!
  Serial.print("Creating model for #");  Serial.println(fid);

  p = finger.createModel();
  if (p == FINGERPRINT_OK) {
    Serial.println("Prints matched!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return p;
  } else if (p == FINGERPRINT_ENROLLMISMATCH) {
    Serial.println("Fingerprints did not match");
    return p;
  } else {
    Serial.println("Unknown error");
    return p;
  }

  Serial.print("ID "); Serial.println(fid);
  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    Serial.println("Stored!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return p;
  } else if (p == FINGERPRINT_BADLOCATION) {
    Serial.println("Could not store in that location");
    return p;
  } else if (p == FINGERPRINT_FLASHERR) {
    Serial.println("Error writing to flash");
    return p;
  } else {
    Serial.println("Unknown error");
    return p;
  }

  fid = finger.fingerID;
  return true;
}