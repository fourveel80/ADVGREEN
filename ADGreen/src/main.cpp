#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <ModbusMaster.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

const char *ssid = "SYNHUB-OFFICE 2.4G";
const char *password = "Syn@tech";
const char *mqtt_server = "172.16.16.58";
const char *device_id = "7smthpg3vu53m8h8aanh";     // Pond1
const char *device_token = "5vwjfrx5k5go92pmqsaf";  //
const char *device_pass = "1pfa0czgnugpv6fsec9a";
// const char *device_id = "spidvj87l53yzxktgjts";     // Pond2
// const char *device_token = "1aady9k7iq2xuyf73qz3";  //
// const char *device_pass = "rjg98xxfxdw0sbwpjyko";
const char *topic_Attributes = "v1/devices/me/attributes";
const char *topic_Telemetry = "v1/devices/me/telemetry";
const char *topic_req = "v1/devices/me/rpc/request/+";
const char *headtopic_res = "v1/devices/me/rpc/response/";

Preferences prefs;

#define PUMP01_OPEN     "Pump1Open"
#define PUMP01_CLOSE    "Pump1Close"
#define PUMP01_SELECT   "Pump1Select"
#define PUMP02_OPEN     "Pump2Open"
#define PUMP02_CLOSE    "Pump2Close"
#define PUMP02_SELECT   "Pump2Select"
#define LIGHT01_OPEN    "Light1Open"
#define LIGHT01_CLOSE   "Light1Close"
#define LIGHT01_SELECT  "Light1Select"
#define LIGHT02_OPEN    "Light2Open"
#define LIGHT02_CLOSE   "Light2Close"
#define LIGHT02_SELECT  "Light2Select"

#define SETVALUE_BTN    "setValueBtn"

#define ON_   true
#define OFF_  false

#define RELAY_01        0
#define RELAY_02        1
#define RELAY_03        2
#define RELAY_04        3

const long interval = 20000;
unsigned long previousMillis = 0;
const long intervalRun = 10000;
unsigned long previousRun  = 0;

TaskHandle_t TaskWork, TaskConnect;

JsonDocument docMode;
JsonDocument docSet;
JsonDocument docRec;
JsonDocument docVal;
JsonDocument docStat;
IPAddress ip;

String topicCheck;

ModbusMaster rsET_SHT31_BH1750, rsRika_EC, rsRika_PH;

float tempVal, humiVal, luxVal, tempwaterVal,
      ecVal, phVal, tempphVal, resisVal, tdsVal;

char data[255];
char msg[255];
char payloadVal[255];
char payloadStat[255];
char topic_res[255];

int buttonStat[4] = {0, 0, 0, 0};

int settingValStat[9] = {38, 28, 
                         38, 28, 
                         38, 28,
                         38, 28, 
                         0};

const char *settingFlagStat[4] = {"temp", "temp", "temp", "temp"};

String getFlag[9] = {"temp", "temp", "temp", "temp"};

const char *method;
int indexButton;
bool valueButton;

int indexSwitch;
bool valueSwitch;

uint16_t cnt_t = 0;
uint16_t flag = 0;

WiFiClient espClient;
PubSubClient mqtt_client(espClient);

const int manualBtn[4] = { 36, 39, 34, 35 };
const int relayPin[4] = { 32, 33, 25, 26 };

int buttonADC() {
  if (digitalRead(manualBtn[0]) == LOW) {
    return 1;
  } else if (digitalRead(manualBtn[1]) == LOW) {
    return 2;
  } else if (digitalRead(manualBtn[2]) == LOW) {
    return 3;
  } else if (digitalRead(manualBtn[3]) == LOW) {
    return 4;
  }
  return 0;
}

void getValuePrefs() {
  prefs.begin("setting", false);
  settingValStat[0] = prefs.getInt(PUMP01_OPEN, 0);
  settingValStat[1] = prefs.getInt(PUMP01_CLOSE, 0);

  settingValStat[2] = prefs.getInt(PUMP02_OPEN, 0);
  settingValStat[3] = prefs.getInt(PUMP02_CLOSE, 0);

  settingValStat[4] = prefs.getInt(LIGHT01_OPEN, 0);
  settingValStat[5] = prefs.getInt(LIGHT01_CLOSE, 0);

  settingValStat[6] = prefs.getInt(LIGHT02_OPEN, 0);
  settingValStat[7] = prefs.getInt(LIGHT02_CLOSE, 0);

  getFlag[0] = prefs.getString(PUMP01_SELECT, "");
  getFlag[1] = prefs.getString(PUMP02_SELECT, "");
  getFlag[2] = prefs.getString(LIGHT01_SELECT, "");
  getFlag[3] = prefs.getString(LIGHT02_SELECT, "");
  prefs.end();

  settingFlagStat[0] = getFlag[0].c_str();
  settingFlagStat[1] = getFlag[1].c_str();
  settingFlagStat[2] = getFlag[2].c_str();
  settingFlagStat[3] = getFlag[3].c_str();
}

void valueUpdate()
{
  prefs.begin("setting", false);
  prefs.putInt(PUMP01_OPEN, settingValStat[0]);
  prefs.putInt(PUMP01_CLOSE, settingValStat[1]);

  prefs.putInt(PUMP02_OPEN, settingValStat[2]);
  prefs.putInt(PUMP02_CLOSE, settingValStat[3]);

  prefs.putInt(LIGHT01_OPEN, settingValStat[4]);
  prefs.putInt(LIGHT01_CLOSE, settingValStat[5]);

  prefs.putInt(LIGHT02_OPEN, settingValStat[6]);
  prefs.putInt(LIGHT02_CLOSE, settingValStat[7]);

  prefs.putString(PUMP01_SELECT, settingFlagStat[0]);
  prefs.putString(PUMP02_SELECT, settingFlagStat[1]);
  prefs.putString(LIGHT01_SELECT, settingFlagStat[2]);
  prefs.putString(LIGHT02_SELECT, settingFlagStat[3]);
  prefs.end();

  docVal["water_temp"] = String(tempwaterVal, 2);
  docVal["ec"] =  String(ecVal, 2);
  docVal["ph"] =  String(phVal, 2);

  docVal["temp"] =  String(tempVal, 2);
  docVal["humi"] =  String(humiVal, 2);
  docVal["light"] =  String(luxVal, 2);

  serializeJson(docVal, payloadVal, 255);
}

void measurementsRS485() {
  uint8_t resultRika_PH = 0;
  uint32_t combinedPH, combinedTempPH;

  uint8_t resultRika_EC = 0;
  uint32_t combinedEC, combinedResis, combinedTempWater, combinedTDS;

  uint8_t resultET_SHT31_BH1750 = 0;
  uint32_t combinedTemp, combinedHumi, combinedLux;
  
  // ==========================================================

  delay(100);

  // RK500-12 PH Sensor
  rsRika_PH.begin(3, Serial2);
  resultRika_PH = rsRika_PH.readHoldingRegisters(0, 6);
  if (resultRika_PH == rsRika_PH.ku8MBSuccess) {
    combinedPH =      ((uint32_t)(rsRika_PH.getResponseBuffer(0)) << 16) | (rsRika_EC.getResponseBuffer(1));
    combinedTempPH =  ((uint32_t)(rsRika_EC.getResponseBuffer(4)) << 16) | (rsRika_EC.getResponseBuffer(5));
    memcpy(&phVal, &combinedPH, sizeof(float));
    memcpy(&tempphVal, &combinedTempPH, sizeof(float));
  }
  else {
    int error = 0;
    Serial.println(error);
  }

  delay(100);

  // RK500-13 EC Sensor
  rsRika_EC.begin(4, Serial2);
  resultRika_EC = rsRika_EC.readHoldingRegisters(0, 8);
  if (resultRika_EC == rsRika_EC.ku8MBSuccess) {
    combinedEC =          ((uint32_t)(rsRika_EC.getResponseBuffer(0)) << 16) | (rsRika_EC.getResponseBuffer(1));
    combinedResis =       ((uint32_t)(rsRika_EC.getResponseBuffer(2)) << 16) | (rsRika_EC.getResponseBuffer(3));
    combinedTempWater =   ((uint32_t)(rsRika_EC.getResponseBuffer(4)) << 16) | (rsRika_EC.getResponseBuffer(5));
    combinedTDS =         ((uint32_t)(rsRika_EC.getResponseBuffer(6)) << 16) | (rsRika_EC.getResponseBuffer(7));
    memcpy(&ecVal, &combinedEC, sizeof(float));
    memcpy(&resisVal, &combinedResis, sizeof(float));
    memcpy(&tempwaterVal, &combinedTempWater, sizeof(float));
    memcpy(&tdsVal, &combinedTDS, sizeof(float));
  } else {
    int error = 1;
    Serial.println(error);
  }

  delay(100);  

  // ET-Modbus RTU SHT31 & BH1750 BOX V3
  rsET_SHT31_BH1750.begin(247, Serial2);
  resultET_SHT31_BH1750 = rsET_SHT31_BH1750.readInputRegisters(5, 8);
  if (resultET_SHT31_BH1750 == rsET_SHT31_BH1750.ku8MBSuccess) {
    combinedTemp =  ((uint32_t)(rsET_SHT31_BH1750.getResponseBuffer(0)) << 16) | (rsET_SHT31_BH1750.getResponseBuffer(1));
    combinedHumi =  ((uint32_t)(rsET_SHT31_BH1750.getResponseBuffer(2)) << 16) | (rsET_SHT31_BH1750.getResponseBuffer(3));
    combinedLux =   ((uint32_t)(rsET_SHT31_BH1750.getResponseBuffer(6)) << 16) | (rsET_SHT31_BH1750.getResponseBuffer(7));
    memcpy(&tempVal, &combinedTemp, sizeof(float));
    memcpy(&humiVal, &combinedHumi, sizeof(float));
    memcpy(&luxVal, &combinedLux, sizeof(float));
  } else {
    int error = 3;
    Serial.println(error);
  }
}

void confirmStatus() {
  JsonArray dataBtn = docStat["button"].to<JsonArray>();
  for (int i = 0; i < 4; i++)
  {
    dataBtn.add(buttonStat[i]); // "button":[0,0,0,1]
  }

  serializeJson(docStat, payloadStat, 255);
  mqtt_client.publish(topic_Attributes, payloadStat);
  Serial.println(payloadStat);
}

void updateStatus(int indexInput, bool valueInput, const char *flag) {
  if(strcmp(flag, "setValueBtn") == 0 || strcmp(flag, "setValueAdc") == 0) {
    indexButton = indexInput;
    valueButton = valueInput;
    if (valueButton == true) {
      indexSwitch = indexButton;
      valueSwitch = false;
    }
    else if (valueButton == false && strcmp(flag, "setValueAdc") == 0) {
      indexSwitch = indexButton;
      valueSwitch = false;
    }
  }
  
  buttonStat[indexButton] = valueButton;

  JsonArray dataBtn = docStat["button"].to<JsonArray>();
  for (int i = 0; i < 4; i++)
  {
    dataBtn.add(buttonStat[i]); // "button":[0,0,0,1]
  }
  
  serializeJson(docStat, payloadStat, 255);
  
  mqtt_client.publish(topic_Attributes, payloadStat);
  Serial.println(payloadStat);
}

void relayActive(int indexRelay, bool valueIndex, const char *flag) {

  if (strcmp(flag,"setValueBtn") == 0 || strcmp(flag, "setValueAdc") == 0) {
    digitalWrite(relayPin[indexRelay], valueIndex);
  }
  if (strcmp(flag,"setValueSwt") == 0) {
    if (valueIndex == true) {
      digitalWrite(relayPin[indexRelay], valueIndex);
    } else {
      digitalWrite(relayPin[indexRelay], valueIndex);
    }
  }
  updateStatus(indexRelay, valueIndex, flag);
}

void pumpAlphaCheck(const char *flagCheck, int valSetOpen, int valSetClose) {
  if (strcmp(flagCheck,"temp") == 0) {
    if (tempVal > valSetOpen) {
      if (buttonStat[RELAY_01] == 0) {
        relayActive(RELAY_01, ON_, SETVALUE_BTN);
      }
    }
    else if (tempVal < valSetClose) {
      if (buttonStat[RELAY_01] == 1) {
        relayActive(RELAY_01, OFF_, SETVALUE_BTN);
      }
    }
  } else if (strcmp(flagCheck,"humi") == 0) {
    if (humiVal < valSetOpen) {
      if (buttonStat[RELAY_01] == 0) {
        relayActive(RELAY_01, ON_, SETVALUE_BTN);
      }
    }
    else if (humiVal > valSetClose) {
      if (buttonStat[RELAY_01] == 1) {
        relayActive(RELAY_01, OFF_, SETVALUE_BTN);
      }
    }
  } else if (strcmp(flagCheck,"lux") == 0) {
    if (luxVal > valSetOpen) {
      if (buttonStat[RELAY_01] == 0) {
        relayActive(RELAY_01, ON_, SETVALUE_BTN);
      }
    }
    else if (luxVal < valSetClose) {
      if (buttonStat[RELAY_01] == 1) {
        relayActive(RELAY_01, OFF_, SETVALUE_BTN);
      }
    }
  } else if (strcmp(flagCheck,"watertemp") == 0) {
    if (tempwaterVal > valSetOpen) {
      if (buttonStat[RELAY_01] == 0) {
        relayActive(RELAY_01, ON_, SETVALUE_BTN);
      }
    }
    else if (tempwaterVal < valSetClose) {
      if (buttonStat[RELAY_01] == 1) {
        relayActive(RELAY_01, OFF_, SETVALUE_BTN);
      }
    }
  } else if (strcmp(flagCheck,"ec") == 0) {
    if (ecVal < valSetOpen) {
      if (buttonStat[RELAY_01] == 0) {
        relayActive(RELAY_01, ON_, SETVALUE_BTN);
      }
    }
    else if (ecVal > valSetClose) {
      if (buttonStat[RELAY_01] == 1) {
        relayActive(RELAY_01, OFF_, SETVALUE_BTN);
      }
    }
  } else if (strcmp(flagCheck,"ph") == 0) {
    if (phVal < valSetOpen) {
      if (buttonStat[RELAY_01] == 0) {
        relayActive(RELAY_01, ON_, SETVALUE_BTN);
      }
    }
    else if (phVal > valSetClose) {
      if (buttonStat[RELAY_01] == 1) {
        relayActive(RELAY_01, OFF_, SETVALUE_BTN);
      }
    }
  }
  delay(50);
}

void pumpBetaCheck(const char *flagCheck, int valSetOpen, int valSetClose) {
  if (strcmp(flagCheck,"temp") == 0) {
    if (tempVal > valSetOpen) {
      if (buttonStat[RELAY_02] == 0) {
        relayActive(RELAY_02, ON_, SETVALUE_BTN);
      }
    }
    else if (tempVal < valSetClose) {
      if (buttonStat[RELAY_02] == 1) {
        relayActive(RELAY_02, OFF_, SETVALUE_BTN);
      }
    }
  } else if (strcmp(flagCheck,"humi") == 0) {
    if (humiVal < valSetOpen) {
      if (buttonStat[RELAY_02] == 0) {
        relayActive(RELAY_02, ON_, SETVALUE_BTN);
      }
    }
    else if (humiVal > valSetClose) {
      if (buttonStat[RELAY_02] == 1) {
        relayActive(RELAY_02, OFF_, SETVALUE_BTN);
      }
    }
  } else if (strcmp(flagCheck,"lux") == 0) {
    if (luxVal > valSetOpen) {
      if (buttonStat[RELAY_02] == 0) {
        relayActive(RELAY_02, ON_, SETVALUE_BTN);
      }
    }
    else if (luxVal < valSetClose) {
      if (buttonStat[RELAY_02] == 1) {
        relayActive(RELAY_02, OFF_, SETVALUE_BTN);
      }
    }
  } else if (strcmp(flagCheck,"watertemp") == 0) {
    if (tempwaterVal > valSetOpen) {
      if (buttonStat[RELAY_02] == 0) {
        relayActive(RELAY_02, ON_, SETVALUE_BTN);
      }
    }
    else if (tempwaterVal < valSetClose) {
      if (buttonStat[RELAY_02] == 1) {
        relayActive(RELAY_02, OFF_, SETVALUE_BTN);
      }
    }
  } else if (strcmp(flagCheck,"ec") == 0) {
    if (ecVal < valSetOpen) {
      if (buttonStat[RELAY_02] == 0) {
        relayActive(RELAY_02, ON_, SETVALUE_BTN);
      }
    }
    else if (ecVal > valSetClose) {
      if (buttonStat[RELAY_02] == 1) {
        relayActive(RELAY_02, OFF_, SETVALUE_BTN);
      }
    }
  } else if (strcmp(flagCheck,"ph") == 0) {
    if (phVal < valSetOpen) {
      if (buttonStat[RELAY_02] == 0) {
        relayActive(RELAY_02, ON_, SETVALUE_BTN);
      }
    }
    else if (phVal > valSetClose) {
      if (buttonStat[RELAY_02] == 1) {
        relayActive(RELAY_02, OFF_, SETVALUE_BTN);
      }
    }
  }
  delay(50);
}

void lightAlphaCheck(const char *flagCheck, int valSetOpen, int valSetClose) {
  if (strcmp(flagCheck,"temp") == 0) {
    if (tempVal > valSetOpen) {
      if (buttonStat[RELAY_03] == 0) {
        relayActive(RELAY_03, ON_, SETVALUE_BTN);
      }
    }
    else if (tempVal < valSetClose) {
      if (buttonStat[RELAY_03] == 1) {
        relayActive(RELAY_03, OFF_, SETVALUE_BTN);
      }
    }
  } else if (strcmp(flagCheck,"humi") == 0) {
    if (humiVal < valSetOpen) {
      if (buttonStat[RELAY_03] == 0) {
        relayActive(RELAY_03, ON_, SETVALUE_BTN);
      }
    }
    else if (humiVal > valSetClose) {
      if (buttonStat[RELAY_03] == 1) {
        relayActive(RELAY_03, OFF_, SETVALUE_BTN);
      }
    }
  } else if (strcmp(flagCheck,"lux") == 0) {
    if (luxVal > valSetOpen) {
      if (buttonStat[RELAY_03] == 0) {
        relayActive(RELAY_03, ON_, SETVALUE_BTN);
      }
    }
    else if (luxVal < valSetClose) {
      if (buttonStat[RELAY_03] == 1) {
        relayActive(RELAY_03, OFF_, SETVALUE_BTN);
      }
    }
  } else if (strcmp(flagCheck,"watertemp") == 0) {
    if (tempwaterVal > valSetOpen) {
      if (buttonStat[RELAY_03] == 0) {
        relayActive(RELAY_03, ON_, SETVALUE_BTN);
      }
    }
    else if (tempwaterVal < valSetClose) {
      if (buttonStat[RELAY_03] == 1) {
        relayActive(RELAY_03, OFF_, SETVALUE_BTN);
      }
    }
  } else if (strcmp(flagCheck,"ec") == 0) {
    if (ecVal < valSetOpen) {
      if (buttonStat[RELAY_03] == 0) {
        relayActive(RELAY_03, ON_, SETVALUE_BTN);
      }
    }
    else if (ecVal > valSetClose) {
      if (buttonStat[RELAY_03] == 1) {
        relayActive(RELAY_03, OFF_, SETVALUE_BTN);
      }
    }
  } else if (strcmp(flagCheck,"ph") == 0) {
    if (phVal < valSetOpen) {
      if (buttonStat[RELAY_03] == 0) {
        relayActive(RELAY_03, ON_, SETVALUE_BTN);
      }
    }
    else if (phVal > valSetClose) {
      if (buttonStat[RELAY_03] == 1) {
        relayActive(RELAY_03, OFF_, SETVALUE_BTN);
      }
    }
  }
  delay(50);
}

void lightBetaCheck(const char *flagCheck, int valSetOpen, int valSetClose) {
  if (strcmp(flagCheck,"temp") == 0) {
    if (tempVal > valSetOpen) {
      if (buttonStat[RELAY_04] == 0) {
        relayActive(RELAY_04, ON_, SETVALUE_BTN);
      }
    }
    else if (tempVal < valSetClose) {
      if (buttonStat[RELAY_04] == 1) {
        relayActive(RELAY_04, OFF_, SETVALUE_BTN);
      }
    }
  } else if (strcmp(flagCheck,"humi") == 0) {
    if (humiVal < valSetOpen) {
      if (buttonStat[RELAY_04] == 0) {
        relayActive(RELAY_04, ON_, SETVALUE_BTN);
      }
    }
    else if (humiVal > valSetClose) {
      if (buttonStat[RELAY_04] == 1) {
        relayActive(RELAY_04, OFF_, SETVALUE_BTN);
      }
    }
  } else if (strcmp(flagCheck,"lux") == 0) {
    if (luxVal > valSetOpen) {
      if (buttonStat[RELAY_04] == 0) {
        relayActive(RELAY_04, ON_, SETVALUE_BTN);
      }
    }
    else if (luxVal < valSetClose) {
      if (buttonStat[RELAY_04] == 1) {
        relayActive(RELAY_04, OFF_, SETVALUE_BTN);
      }
    }
  } else if (strcmp(flagCheck,"watertemp") == 0) {
    if (tempwaterVal > valSetOpen) {
      if (buttonStat[RELAY_04] == 0) {
        relayActive(RELAY_04, ON_, SETVALUE_BTN);
      }
    }
    else if (tempwaterVal < valSetClose) {
      if (buttonStat[RELAY_04] == 1) {
        relayActive(RELAY_04, OFF_, SETVALUE_BTN);
      }
    }
  } else if (strcmp(flagCheck,"ec") == 0) {
    if (ecVal < valSetOpen) {
      if (buttonStat[RELAY_04] == 0) {
        relayActive(RELAY_04, ON_, SETVALUE_BTN);
      }
    }
    else if (ecVal > valSetClose) {
      if (buttonStat[RELAY_04] == 1) {
        relayActive(RELAY_04, OFF_, SETVALUE_BTN);
      }
    }
  } else if (strcmp(flagCheck,"ph") == 0) {
    if (phVal < valSetOpen) {
      if (buttonStat[RELAY_04] == 0) {
        relayActive(RELAY_04, ON_, SETVALUE_BTN);
      }
    }
    else if (phVal > valSetClose) {
      if (buttonStat[RELAY_04] == 1) {
        relayActive(RELAY_04, OFF_, SETVALUE_BTN);
      }
    }
  }
  delay(50);
}

void convertJsonSetting(const char *package) {
  DeserializationError error = deserializeJson(docSet, package);
  int settingVal[] = {
    docSet[PUMP01_OPEN],
    docSet[PUMP01_CLOSE],
    docSet[PUMP02_OPEN],
    docSet[PUMP02_CLOSE],
    docSet[LIGHT01_OPEN],
    docSet[LIGHT01_CLOSE],
    docSet[LIGHT02_OPEN],
    docSet[LIGHT02_CLOSE],
  };

   const char *settingFlag[] = {
    docSet[PUMP01_SELECT],
    docSet[PUMP02_SELECT],
    docSet[LIGHT01_SELECT],
    docSet[LIGHT02_SELECT],
  };

  for (int i = 0; i < sizeof(settingVal) / sizeof(settingVal[0]); i++) {
    if (settingVal[i] > 0) {
      settingValStat[i] = settingVal[i];
    }
    Serial.print("Value ");
    Serial.print(i + 1);
    Serial.print(": ");
    Serial.println(settingValStat[i]);
  }

  for (int i = 0; i < sizeof(settingFlag) / sizeof(settingFlag[0]); i++) {
    const char *curSettiing = settingFlag[i];
    const char *oldSetting = getFlag[i].c_str();
    if (curSettiing != "" && curSettiing != NULL) {
      settingFlagStat[i] = curSettiing;
      getFlag[i] = curSettiing;
    } else {
      settingFlagStat[i] = oldSetting;
    }
    Serial.print("Flag ");
    Serial.print(i + 1);
    Serial.print(": ");
    Serial.println(settingFlagStat[i]);
  }
}

void convertJsonAction(const char *msg)
{
  // >>> msg = {"method":"setValue","params":{"button":0,"value":true}}
  DeserializationError error = deserializeJson(docRec, msg);

  method = docRec["method"];
  if (strcmp(method,"setValueBtn") == 0 && settingValStat[8] == 1) {
    indexButton = docRec["params"]["button"];
    valueButton = docRec["params"]["valueBtn"];
    relayActive(indexButton, valueButton, method);
  } else if (strcmp(method,"setModeBtn") == 0) {
    settingValStat[8] = docRec["params"]["valueMode"];
    JsonArray dataMode = docMode["mode"].to<JsonArray>();
    dataMode.add(settingValStat[8]);
    serializeJson(docMode, payloadStat, 255);
    mqtt_client.publish(topic_Attributes, payloadStat);
    Serial.println(payloadStat);
  } else {
    Serial.println("IN AUTO MODE CANT CONTROL MAN BUTTON!");
    confirmStatus();
  }
}

void callback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Message arrived [");
  Serial.print(topic); //  v1/devices/me/rpc/request/0
  Serial.print("] ");
  for (int i = 0; i < length; i++)
  {
    msg[i] = (char)payload[i];
    // Serial.print((char)payload[i]);
  }

  topicCheck = topic;
  Serial.println();
  Serial.println(msg);

  char *token = strrchr(topic, '/');
  char prt[255];

  if (token != NULL)
  {
    // Serial.println(token + 1);
    strcpy(topic_res, headtopic_res);
    strcat(topic_res, token + 1);
  }
  strcpy(prt, "Topic respones : ");
  strcat(prt, topic_res);
  // Serial.println(prt);
  
  sprintf(payloadVal, "{\"method\":%s, \"params\":%s}", "\"getValue\"", "\"OK\"");
  mqtt_client.publish(topic_res, payloadVal);
  Serial.println(payloadVal);
}

void reconnect() {
  if (!mqtt_client.connected()) {
    Serial.println("Attempting MQTT connection...");
    if (mqtt_client.connect(device_id, device_token, device_pass)) {
      mqtt_client.subscribe("v1/devices/me/attributes");
      mqtt_client.subscribe(topic_req);
      Serial.println("MQTT Connected");
      confirmStatus();
    } else {
      Serial.print("Failed, rc=");
      Serial.print(mqtt_client.state());
      if ((WiFi.status() != WL_CONNECTED)) {
        WiFi.begin(ssid, password);
        Serial.println("Connecting to WiFi...");
        delay(200);
        if (WiFi.status() != WL_CONNECTED) {
          Serial.println("WiFi Disconnected...");
          Serial.println("Reset in 1 seconds...");
          delay(200);
          ESP.restart();
        }
      }
      Serial.println(" Retrying in 5 seconds...");
    }
  }
}

void TaskWorkPart(void *parameter)
{
  while (true)
  {
    int valMan = buttonADC();
    if (valMan > 0) {
      delay(100);
      if (buttonStat[valMan - 1] == 1 && settingValStat[8] == 1) {
        relayActive(indexButton = (valMan - 1),valueButton = 0, "setValueAdc");
      } else {
        relayActive(indexButton = (valMan - 1),valueButton = 1, "setValueAdc");
      }
      while (buttonADC() > 0) {
        delay(1);
      }
    }

    if (strlen(msg) != 0) {
      if (topicCheck == "v1/devices/me/attributes") {
        Serial.println("Get Setting Value [");
        convertJsonSetting(msg);
        Serial.println("]");
      }
      else {
        convertJsonAction(msg);
      }
      memset(&msg[0], 0, sizeof(msg)); //for clear data
    }

    unsigned long currentTime_Run = millis();
    if (currentTime_Run - previousRun >= intervalRun)
    {
      measurementsRS485();
      valueUpdate();
      if (settingValStat[8] == 0) {
        pumpAlphaCheck(settingFlagStat[0], settingValStat[0], settingValStat[1]);
        pumpBetaCheck(settingFlagStat[1], settingValStat[2], settingValStat[3]);
        lightAlphaCheck(settingFlagStat[2], settingValStat[4], settingValStat[5]);
        lightBetaCheck(settingFlagStat[3], settingValStat[6], settingValStat[7]);
      }
      previousRun = currentTime_Run;
    }
    delay(50);
  }
}

void TaskConnectPart(void *parameter)
{
  while (true)
  {
    if (!mqtt_client.connected()) {
      reconnect();
    }
    
    unsigned long currentTime_updateData = millis();
    if (currentTime_updateData - previousMillis >= interval)
    {
      confirmStatus();
      mqtt_client.publish(topic_Telemetry, payloadVal);
      Serial.print("Publish: ");
      Serial.println(payloadVal);
      previousMillis = currentTime_updateData;
    }
    delay(50);
    mqtt_client.loop();
  }
}

void setup()
{
  Wire.begin();
  Serial.begin(115200);
  Serial2.begin(9600); 
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  WiFi.begin(ssid, password);
  delay(1000);
  if ((WiFi.status() != WL_CONNECTED)) {
    WiFi.begin(ssid, password);
    Serial.println("Connecting to WiFi...");
    delay(1000);
    int cnt = 10;
    while (WiFi.status() != WL_CONNECTED) {
      WiFi.begin(ssid, password);
      delay(1000);
      Serial.println("Connecting to WiFi...");
      if (cnt == 0) {
        Serial.println("Reset..");
        ESP.restart();
      }
      cnt--;  
    }
    mqtt_client.setServer(mqtt_server, 1883);
    mqtt_client.setCallback(callback);
  }

  delay(1000);

  getValuePrefs();

  pinMode(manualBtn[0], INPUT);
  pinMode(manualBtn[1], INPUT);
  pinMode(manualBtn[2], INPUT);
  pinMode(manualBtn[3], INPUT);

  pinMode(relayPin[0], OUTPUT);
  pinMode(relayPin[1], OUTPUT);
  pinMode(relayPin[2], OUTPUT);
  pinMode(relayPin[3], OUTPUT);

  digitalWrite(relayPin[0], LOW);
  digitalWrite(relayPin[1], LOW);
  digitalWrite(relayPin[2], LOW);
  digitalWrite(relayPin[3], LOW);  

  xTaskCreatePinnedToCore(TaskConnectPart, "TaskConnect", 8192, NULL, 0, &TaskConnect, 0);
  delay(1000);
  xTaskCreatePinnedToCore(TaskWorkPart, "TaskWork", 8192, NULL, 1, &TaskWork, 1);
}

void loop()
{
}
