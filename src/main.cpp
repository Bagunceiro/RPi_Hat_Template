#include <Arduino.h>

#include <espnow.h>
#include <ESP8266WiFi.h>

#include <ArduinoJson.h>

uint8_t receiverMac[] = {0x24, 0xa1, 0x60, 0x30, 0xa1, 0x0d};

const int MAXINSIGHT = 250;

void log(const char *m)
{
  char dbuffer[256];
  unsigned int serialno = 0;
  StaticJsonDocument<1024> data;
  data["sn"] = serialno++;
  data["op"] = "log";
  data["mesg"] = m;
  serializeJson(data, dbuffer);
  StaticJsonDocument<1024> doc;
  doc["sender"] = "";
  doc["len"] = strlen(m);
  doc["data"] = dbuffer;
  serializeJson(doc, Serial);
  Serial.println();
}

void onSent(uint8_t *mac_addr, uint8_t sendStatus)
{
  char buffer[16];
  sprintf(buffer, "sent (%d)", sendStatus);
  // log(buffer);
  // Success if sendStatus == 0
}

char *mac2str(const uint8_t *mac, char* out)
{

  for (int i = 0; i < 6; i++)
  {
    sprintf(out + (3 * i), "%02x:", mac[i]);
  }
  out[17] = '\0'; // clobber the last colon
  return out;
}

uint8_t* str2mac(const char* str, uint8_t* out)
{
  sscanf(str, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &out[0], &out[1], &out[2], &out[3], &out[4], &out[5]);
  return out;
}

char *escapeStr(const uint8_t *d, const int len, char *buffer)
{
  int pos = 0;
  for (int i = 0; i < len; i++)
  {
    unsigned char c = d[i];
    if (isprint(c) && c != '"' && c != '\\')
      buffer[pos++] = c;
    else
    {
      buffer[pos++] = '\\'; // add an escape
      char num[8];
      bool usenum = false;

      switch (c)
      {
      case ('"'):
      case ('\\'):
        break; // no change to these (just the escape above)
      case ('\b'):
        c = 'b';
        break;
      case ('\n'):
        c = 'n';
        break;
      case ('\r'):
        c = 'r';
        break;
      case ('\t'):
        c = 't';
        break;
      default:
        sprintf(num, "u%04x", c);
        usenum = true;
      }
      if (usenum)
      {
        strcpy(buffer + pos, num);
        pos = strlen(buffer);
      }
      else
        buffer[pos++] = c;
    }
  }
  buffer[pos] = '\00';
  return buffer;
}

void onRecv(uint8_t *mac, uint8_t *data, const uint8_t len)
{
  // log("onRecv");
  char macaddr[24];
  mac2str(mac, macaddr);
  unsigned char buffer[len + 1];
  memcpy(buffer, data, len);
  buffer[len] = '\00';

  StaticJsonDocument<1024> doc;
  
  doc["sender"] = macaddr;
  doc["len"] = len;
  doc["data"] = buffer;

  serializeJson(doc, Serial);
  Serial.println();
}

void forward(const char* msg)
{
  StaticJsonDocument<1024> doc;
  deserializeJson(doc, msg);
  const char* recipient = doc["recipient"];
  
  const char* data;
  if (doc["data"].is<const char*>())
  {
      data = doc["data"];
  }
  else
  if (doc["data"].is<JsonObject&>())
  {
    data = "Is an object";
  }
  else
  {
    data = "dunno";
  }

  uint8_t mac[6];

  str2mac(recipient, mac);

  esp_now_add_peer(mac,ESP_NOW_ROLE_COMBO, 1, NULL, 0);

  
  esp_now_send(mac, (u8*)data, strlen(data));
}

void setup()
{
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);

  // Init ESP-NOW
  if (esp_now_init() != 0)
  {
    // Gawd knows .. for now:
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);

  esp_now_register_recv_cb(onRecv);

  char buffer[32];
  sprintf(buffer, "MAC=%s", WiFi.macAddress().c_str());
  log(buffer);

  esp_now_register_send_cb(onSent);
  /*
 esp_now_add_peer(receiverMac, ESP_NOW_ROLE_SLAVE, 1, NULL, 0);
 */
}

void loop()
{
  static char buffer[MAXINSIGHT + 1];
  static int len = 0;
  static unsigned long then = 0;
  int c = Serial.read();
  if (c >= 0)
  {

    then = millis();
    if (c != '\n')
    {
      buffer[len++] = c;
      buffer[len] = 0;
      if (len > MAXINSIGHT) // buffer overflow - force flush
      {
        c = '\n';
      }
    }
    if (c == '\n') // flush the buffer
    {
      forward(buffer);
      len = 0;
      then = 0;
    }
  }
  else
  {
    if (len != 0)
    {
      if (millis() - then > 5000)
      {
          forward(buffer);
          len = 0;
          then = millis();
      }
    }
  }
}