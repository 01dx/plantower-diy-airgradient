#ifndef _LOCAL_SERVER_H_
#define _LOCAL_SERVER_H_

#include "AgConfigure.h"
#include "AgValue.h"
#include "AirGradient.h"
#include "OpenMetrics.h"
#include "AgWiFiConnector.h"
#include <Arduino.h>
#include <ESP8266WebServer.h>

class LocalServer : public PrintLog {
private:
  AirGradient *ag;
  OpenMetrics &openMetrics;
  Measurements &measure;
  Configuration &config;
  WifiConnector &wifiConnector;
  ESP8266WebServer server;
  AgFirmwareMode fwMode;
  uint32_t plantowerLastReadingMs = 0;
  uint32_t plantowerNextReadingMs = 0;
  float openMeteoLat = 0;
  float openMeteoLon = 0;
  String tempHumSource = "open-meteo";
  void (*saveOpenMeteoLocation)(float, float) = nullptr;
  String formatApiTemp(float value);
  String formatApiRhum(float value);
  String tempHumSourceLabel(void);
  bool correctedPm25(float &out);

  String pageShell(const char *activeTab, const String &body);
  String formatPmValue(int value, bool particleCount = false);
  String wifiSignalLabel(int rssi);
  String currentSsid(void);
  void sendHtml(const String &html);
  void rebootAfterReply(const String &message);

public:
  LocalServer(Stream &log, OpenMetrics &openMetrics, Measurements &measure,
              Configuration &config, WifiConnector &wifiConnector);
  ~LocalServer();

  bool begin(void);
  void setAirGraident(AirGradient *ag);
  String getHostname(void);
  void setFwMode(AgFirmwareMode fwMode);
  void setPlantowerReadingTiming(uint32_t lastReadingMs,
                                 uint32_t nextReadingMs);
  void setOpenMeteoLocation(float lat, float lon,
                            void (*saveFn)(float, float));
  void setTempHumSource(const char *source);
  void _handle(void);
  void _GET_config(void);
  void _PUT_config(void);
  void _GET_metrics(void);
  void _GET_measure(void);
  void _GET_home(void);
  void _GET_settings(void);
  void _GET_help(void);
  void _GET_plantower_settings(void);
  void _POST_plantower_settings(void);
  void _POST_wifi(void);
  void _POST_wifi_setup(void);
  void _POST_location(void);
};

#endif /** _LOCAL_SERVER_H_ */
