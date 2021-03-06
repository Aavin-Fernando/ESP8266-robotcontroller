#define USE_LittleFS

#ifdef USE_LittleFS
  #include <FS.h>
  #define MYFS LittleFS
  #include <LittleFS.h> 
#else
  #define MYFS SPIFFS
#endif

#include "main.h"


void onBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
  //Handle body
}

void onEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){
  //Handle WebSocket event
  switch (type) {
    case WS_EVT_CONNECT:
      #ifdef DEBUG
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      #endif
      break;

    case WS_EVT_DISCONNECT:
      #ifdef DEBUG
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      #endif
      break;

    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;

    case WS_EVT_DATA:  //data packet

    AwsFrameInfo * info = (AwsFrameInfo*)arg;
    if(info->final && info->index == 0 && info->len == len){
      //the whole message is in a single frame and we got all of it's data
      if(info->opcode == WS_TEXT){
        char dest[3];
        int lentocopy = info->len > 2 ? 2 : info->len;
        memcpy(dest,data,lentocopy);
        dest[lentocopy] = '\0';

        if( strcmp(dest,specialmessage::heartbeat) == 0 ) {
          #ifdef DEBUG
          Serial.printf("HB recieved [%u] \n", client->id());
          #endif
          m_heart.ResetHeart();
        } else {
          data[len] = 0;
          #ifdef DEBUG
          Serial.printf("ws[%s][%u] %s-message[%llu]: %s\n", server->url(), client->id(), (info->opcode == WS_TEXT)?"text":"binary", info->len,(char*)data);
          #endif
          DynamicJsonDocument doc(32); // 16 bytes for each double
          DeserializationError error = deserializeJson(doc, (char*)data, info->len);
          if (error) {
            Serial.print(F("deserializeJson() failed: "));
            Serial.println(error.f_str());
          }

          #ifdef DEBUG
          double lM = doc["lM"]; // remove the warning
          double rM = doc["rM"];
          Serial.printf("commanding left [%f] and right [%f] motors \n", lM,rM); 
          #endif

          m_leftFMotor.Set(doc["lM"]);
          m_rightFMotor.Set(doc["rM"]);
          m_leftRMotor.FollowOnce(&m_leftFMotor);
          m_rightRMotor.FollowOnce(&m_rightFMotor);
          

        }  
      }
      }
      break;
  }
}

void notFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "File Not found");
}



void setup()
{
  
  // initialize LED digital pin as an output.
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN,0);
  // setup AP open
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, NULL, 11, 0, 4);

  Serial.println("");
  Serial.print("We are: ");
  Serial.println(ssid);

  if (MDNS.begin("esp8266")){
    Serial.println("MDNS responder started");
  }

  // Attach websocket
  ws.onEvent(onEvent);
  server.addHandler(&ws);

  server.addHandler(&events);

  if (MYFS.begin()) {
    Serial.print(F("FS mounted\n"));
  } else {
    Serial.print(F("FS mount failed\n"));
    return;  
  }

  server.serveStatic("/", MYFS, "/").setDefaultFile("index.html");

  // send a file when /index is requested
  server.onRequestBody(onBody);
  server.onNotFound(notFound);

  server.begin();
  Serial.println("Webserver live");

  taskManager.scheduleFixedRate(500, [] {
    digitalWrite(LED_BUILTIN,LOW); // this is the idle LED, HIGH = off
    delay(50);
    digitalWrite(LED_BUILTIN,HIGH);
    delay(100);
    digitalWrite(LED_BUILTIN,LOW);
    delay(50);    
    digitalWrite(LED_BUILTIN,HIGH);
  }, TIME_MILLIS);

  taskManager.scheduleFixedRate(1, [] {
    ws.cleanupClients(5);
  }, TIME_SECONDS);

  taskManager.scheduleFixedRate(500, [] {
    if(m_heart.HeartDead()){
      m_leftFMotor.Disable();
      m_leftRMotor.Disable();
      m_rightFMotor.Disable();
      m_rightRMotor.Disable();
      #ifdef DEBUG
      Serial.println("ERR: Motorsafety");
      #endif
    } else {
      m_leftFMotor.Enable();
      m_leftRMotor.Enable();
      m_rightFMotor.Enable();
      m_rightRMotor.Enable();
    }
  });

  // TODO: remove once heartbeat & enable/disable webUI impl
  m_leftFMotor.Enable();
  m_rightFMotor.Enable();
  m_leftRMotor.Enable();
  m_rightRMotor.Enable();

  m_heart.ResetHeart();
}

void loop()
{
  taskManager.runLoop();
}