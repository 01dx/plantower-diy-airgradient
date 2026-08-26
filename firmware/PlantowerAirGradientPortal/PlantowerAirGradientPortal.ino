/*
This is the code for the AirGradient DIY BASIC Air Quality Monitor with an D1
ESP8266 Microcontroller.

Plantower project note: this portal firmware is for NodeMCU ESP8266 +
PMS5003 only. CO2 and gas sensors are disabled. Humidity and temperature
come from Open-Meteo until an onboard sensor is wired. Wi-Fi is entered
in the web UI, never compiled into the firmware.

It is an air quality monitor for PM2.5, CO2, Temperature and Humidity with a
small display and can send data over Wifi.

Open source air quality monitors and kits are available:
Indoor Monitor: https://www.airgradient.com/indoor/
Outdoor Monitor: https://www.airgradient.com/outdoor/

Build Instructions:
https://www.airgradient.com/documentation/diy-v4/

Compile Instructions:
https://github.com/airgradienthq/arduino/blob/master/docs/howto-compile.md

Configuration parameters, e.g. Celsius / Fahrenheit or PM unit (US AQI vs ug/m3)
can be set through the AirGradient dashboard.

If you have any questions please visit our forum at
https://forum.airgradient.com/

CC BY-SA 4.0 Attribution-ShareAlike 4.0 International License

*/

#include "AgApiClient.h"
#include "AgConfigure.h"
#include "AgSchedule.h"
#include "AgWiFiConnector.h"
#include "LocalServer.h"
#include "OpenMetrics.h"
#include "MqttClient.h"
#include <AirGradient.h>
#include <EEPROM.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiClient.h>

#ifdef PLANTOWER_AIRGRADIENT_USE_LOCAL_WIFI
#include "local_wifi_credentials.h"
#endif

#ifdef PLANTOWER_AIRGRADIENT_FORCE_PORTAL
#include <user_interface.h>
#endif

#define LED_BAR_ANIMATION_PERIOD 100                  /** ms */
#define DISP_UPDATE_INTERVAL 2500                     /** ms */
#define SERVER_CONFIG_SYNC_INTERVAL 60000             /** ms */
#define SERVER_SYNC_INTERVAL 600000                   /** ms */
#define MQTT_SYNC_INTERVAL 60000                      /** ms */
#define SENSOR_CO2_CALIB_COUNTDOWN_MAX 5              /** sec */
#define SENSOR_TVOC_UPDATE_INTERVAL 1000              /** ms */
#define SENSOR_CO2_UPDATE_INTERVAL 4000               /** ms */
#define SENSOR_PM_UPDATE_INTERVAL 2000                /** ms */
#define SENSOR_TEMP_HUM_UPDATE_INTERVAL 6000          /** ms */
#define DISPLAY_DELAY_SHOW_CONTENT_MS 2000            /** ms */
#define PLANTOWER_FLASH_BUTTON_PIN 0                  /** NodeMCU FLASH / GPIO0 */
#define PLANTOWER_FLASH_HOLD_MS 3000
#define PLANTOWER_WIFI_FALLBACK_MS 180000
#ifndef PLANTOWER_OPEN_METEO_LAT
#define PLANTOWER_OPEN_METEO_LAT 0.0f
#endif
#ifndef PLANTOWER_OPEN_METEO_LON
#define PLANTOWER_OPEN_METEO_LON 0.0f
#endif
#define PLANTOWER_OPEN_METEO_UPDATE_INTERVAL 600000   /** 10 minutes */
#define PLANTOWER_LOCATION_EEPROM_ADDR 0
#define PLANTOWER_LOCATION_MAGIC 0x504C4F43u          /** PLOC */

static AirGradient ag(DIY_BASIC);
static Configuration configuration(Serial);
static AgApiClient apiClient(Serial, configuration);
static Measurements measurements(configuration);
static OledDisplay oledDisplay(configuration, measurements, Serial);
static StateMachine stateMachine(oledDisplay, Serial, measurements,
                                 configuration);
static WifiConnector wifiConnector(oledDisplay, Serial, stateMachine,
                                   configuration);
static OpenMetrics openMetrics(measurements, configuration, wifiConnector,
                               apiClient);
static LocalServer localServer(Serial, openMetrics, measurements, configuration,
                               wifiConnector);
static MqttClient mqttClient(Serial);

static AgFirmwareMode fwMode = FW_MODE_I_BASIC_40PS;

static String fwNewVersion;
static bool networkServicesStarted = false;
static bool serverConfigurationChecked = false;
static int activeServerSyncInterval = SERVER_SYNC_INTERVAL;
static int activePmUpdateInterval = SENSOR_PM_UPDATE_INTERVAL;
static int activePmsWarmupInterval = 60000;
static bool activePlantowerDutyMode = false;
static bool pmsDutySleeping = false;
static bool pmsDutyWarming = false;
static uint32_t pmsDutyWakeStarted = 0;
static uint32_t pmsDutyLastRead = 0;
static uint32_t pmsRetryAt = 0;
static uint32_t wifiLostSince = 0;
static uint32_t flashHeldSince = 0;
static float openMeteoLat = PLANTOWER_OPEN_METEO_LAT;
static float openMeteoLon = PLANTOWER_OPEN_METEO_LON;

static void boardInit(void);
static void failedHandler(String msg);
static void configurationUpdateSchedule(void);
static void appDispHandler(void);
static void oledDisplaySchedule(void);
static void updateTvoc(void);
static void updatePm(void);
static void sendDataToServer(void);
static void tempHumUpdate(void);
static bool fetchOpenMeteo(void);
static bool parseOpenMeteoCurrent(const String &body, float &temp,
                                  float &rhum);
static void loadOpenMeteoLocation(void);
static void saveOpenMeteoLocation(float lat, float lon);
static void co2Update(void);
static void mdnsInit(void);
static void initMqtt(void);
static void factoryConfigReset(void);
static void wdgFeedUpdate(void);
static bool sgp41Init(void);
static void wifiFactoryConfigure(void);
static void mqttHandle(void);
static int calculateMaxPeriod(int updateInterval);
static void setMeasurementMaxPeriod();
static void startNetworkServicesIfNeeded(void);
static void fetchServerConfigurationIfNeeded(void);
static void applyPlantowerLocalSettings(bool force);
static void handlePlantowerSensor(void);
static void forgetWifiAndReboot(const char *reason);
static void handleFlashButton(void);
static void handleWifiFallback(void);

AgSchedule dispLedSchedule(DISP_UPDATE_INTERVAL, oledDisplaySchedule);
AgSchedule configSchedule(SERVER_CONFIG_SYNC_INTERVAL,
                          configurationUpdateSchedule);
AgSchedule agApiPostSchedule(SERVER_SYNC_INTERVAL, sendDataToServer);
AgSchedule co2Schedule(SENSOR_CO2_UPDATE_INTERVAL, co2Update);
AgSchedule pmsSchedule(SENSOR_PM_UPDATE_INTERVAL, updatePm);
AgSchedule tempHumSchedule(PLANTOWER_OPEN_METEO_UPDATE_INTERVAL, tempHumUpdate);
AgSchedule tvocSchedule(SENSOR_TVOC_UPDATE_INTERVAL, updateTvoc);
AgSchedule watchdogFeedSchedule(60000, wdgFeedUpdate);
AgSchedule mqttSchedule(MQTT_SYNC_INTERVAL, mqttHandle);

void setup() {
  /** Serial for print debug message */
  Serial.begin(115200);
  delay(100); /** For bester show log */

  /** Print device ID into log */
  Serial.println("Serial nr: " + ag.deviceId());

#ifdef PLANTOWER_AIRGRADIENT_FORCE_PORTAL
  Serial.println("Plantower portal mode active.");
  Serial.println("Clearing saved ESP8266 Wi-Fi credentials so setup portal starts.");
  WiFi.persistent(true);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, true);
  delay(500);
  wifi_station_set_auto_connect(false);
  WiFi.persistent(false);
#endif

  /** Initialize local configure */
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.softAPdisconnect(false);

  pinMode(PLANTOWER_FLASH_BUTTON_PIN, INPUT_PULLUP);

  configuration.begin();

  /** Init I2C */
  Wire.begin(ag.getI2cSdaPin(), ag.getI2cSclPin());
  delay(1000);

  configuration.setAirGradient(&ag);
  oledDisplay.setAirGradient(&ag);
  stateMachine.setAirGradient(&ag);
  wifiConnector.setAirGradient(&ag);
  apiClient.setAirGradient(&ag);
  openMetrics.setAirGradient(&ag);
  localServer.setAirGraident(&ag);
  loadOpenMeteoLocation();
  localServer.setOpenMeteoLocation(openMeteoLat, openMeteoLon,
                                   saveOpenMeteoLocation);
  measurements.setAirGradient(&ag);

  /** Example set custom API root URL */
  // apiClient.setApiRoot("https://example.custom.api");

  /** Init sensor */
  boardInit();
  applyPlantowerLocalSettings(true);
  setMeasurementMaxPeriod();

  // Uncomment below line to print every measurements reading update
  // measurements.setDebug(true);

  /** Connecting wifi */
  bool connectToWifi = false;

#ifdef PLANTOWER_AIRGRADIENT_USE_LOCAL_WIFI
  wifiConnector.defaultSsid = PLANTOWER_WIFI_SSID;
  wifiConnector.defaultPassword = PLANTOWER_WIFI_PASSWORD;
  Serial.println("Plantower Wi-Fi override active.");
  Serial.println("Clearing saved ESP8266 Wi-Fi credentials and setting configured network.");
  WiFi.persistent(true);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, true);
  delay(250);
  WiFi.begin(PLANTOWER_WIFI_SSID, PLANTOWER_WIFI_PASSWORD);
  WiFi.persistent(false);
#endif

  connectToWifi = !configuration.isOfflineMode();
  if (connectToWifi) {
    apiClient.begin();

    if (wifiConnector.connect()) {
      if (wifiConnector.isConnected()) {
        startNetworkServicesIfNeeded();
      } else {
        if (wifiConnector.isConfigurePorttalTimeout()) {
          oledDisplay.showRebooting();
          delay(2500);
          oledDisplay.setText("", "", "");
          ESP.restart();
        }
      }
    }
  }
  /** Set offline mode without saving, cause wifi is not configured */
  if (wifiConnector.hasConfigurated() == false) {
    Serial.println("Set offline mode cause wifi is not configurated");
    configuration.setOfflineModeWithoutSave(true);
  }

  /** Show display Warning up */
  String sn = "SN:" + ag.deviceId();
  oledDisplay.setText("Warming Up", sn.c_str(), "");

  delay(DISPLAY_DELAY_SHOW_CONTENT_MS);

  Serial.println("Display brightness: " +
                 String(configuration.getDisplayBrightness()));
  oledDisplay.setBrightness(configuration.getDisplayBrightness());

  appDispHandler();
}

void loop() {
  /** Handle schedule */
  dispLedSchedule.run();
  configSchedule.run();
  agApiPostSchedule.run();

  if (configuration.hasSensorS8) {
    co2Schedule.run();
  }
  handlePlantowerSensor();
  if (configuration.hasSensorSHT) {
    tempHumSchedule.run();
  }
  if (configuration.hasSensorSGP) {
    tvocSchedule.run();
  }

  watchdogFeedSchedule.run();

  /** Check for handle WiFi reconnect */
  wifiConnector.handle();
  startNetworkServicesIfNeeded();
  handleFlashButton();
  handleWifiFallback();

  /** factory reset handle */
  // factoryConfigReset();

  /** check that local configura changed then do some action */
  configUpdateHandle();

  localServer._handle();

  if (configuration.hasSensorSGP) {
    ag.sgp41.handle();
  }

  MDNS.update();

  mqttSchedule.run();
  mqttClient.handle();
}

static void co2Update(void) {
  if (!configuration.hasSensorS8) {
    // Device don't have S8 sensor
    return;
  }

  int value = ag.s8.getCo2();
  if (utils::isValidCO2(value)) {
    measurements.update(Measurements::CO2, value);
  } else {
    measurements.update(Measurements::CO2, utils::getInvalidCO2());
  }
}

static void mdnsInit(void) {
  Serial.println("mDNS init");
  if (!MDNS.begin(localServer.getHostname().c_str())) {
    Serial.println("Init mDNS failed");
    return;
  }

  MDNS.addService("_airgradient", "_tcp", 80);
  MDNS.addServiceTxt("_airgradient", "_tcp", "model",
                     AgFirmwareModeName(fwMode));
  MDNS.addServiceTxt("_airgradient", "_tcp", "serialno", ag.deviceId());
  MDNS.addServiceTxt("_airgradient", "_tcp", "fw_ver", ag.getVersion());
  MDNS.addServiceTxt("_airgradient", "_tcp", "vendor", "AirGradient");

  MDNS.announce();
}

static void fetchServerConfigurationIfNeeded(void) {
  if (serverConfigurationChecked) {
    return;
  }

  serverConfigurationChecked = true;
  if (configuration.getConfigurationControl() !=
      ConfigurationControl::ConfigurationControlLocal) {
    apiClient.fetchServerConfiguration();
  }

  configSchedule.update();
  if (apiClient.isFetchConfigurationFailed()) {
    if (apiClient.isNotAvailableOnDashboard()) {
      stateMachine.displaySetAddToDashBoard();
      stateMachine.displayHandle(AgStateMachineWiFiOkServerOkSensorConfigFailed);
    } else {
      stateMachine.displayClearAddToDashBoard();
    }
    delay(DISPLAY_DELAY_SHOW_CONTENT_MS);
  }
}

static void startNetworkServicesIfNeeded(void) {
  if (networkServicesStarted || !wifiConnector.isConnected()) {
    return;
  }

  wifiConnector.markConfigured();
  WiFi.mode(WIFI_STA);
  WiFi.softAPdisconnect(false);
  mdnsInit();
  localServer.begin();
  networkServicesStarted = true;
  initMqtt();
  sendDataToAg();
  fetchServerConfigurationIfNeeded();
  tempHumUpdate();
}

static void initMqtt(void) {
  String mqttUri = configuration.getMqttBrokerUri();
  if (mqttUri.isEmpty()) {
    Serial.println(
        "MQTT is not configured, skipping initialization of MQTT client");
    return;
  }

  if (mqttClient.begin(mqttUri)) {
    Serial.println("Successfully connected to MQTT broker");
  } else {
    Serial.println("Connection to MQTT broker failed");
  }
}

static void wdgFeedUpdate(void) {
  ag.watchdog.reset();
  Serial.println("External watchdog feed!");
}

static bool sgp41Init(void) {
  ag.sgp41.setNoxLearningOffset(configuration.getNoxLearningOffset());
  ag.sgp41.setTvocLearningOffset(configuration.getTvocLearningOffset());
  if (ag.sgp41.begin(Wire)) {
    Serial.println("Init SGP41 success");
    configuration.hasSensorSGP = true;
    return true;
  } else {
    Serial.println("Init SGP41 failure");
    configuration.hasSensorSGP = false;
  }
  return false;
}

static void forgetWifiAndReboot(const char *reason) {
  Serial.print("Forgetting Wi-Fi and rebooting into setup hotspot: ");
  Serial.println(reason);
  wifiConnector.reset();
  WiFi.persistent(true);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, true);
  delay(250);
  WiFi.persistent(false);
  ESP.restart();
}

static void handleFlashButton(void) {
  if (digitalRead(PLANTOWER_FLASH_BUTTON_PIN) == LOW) {
    if (flashHeldSince == 0) {
      flashHeldSince = millis();
    } else if ((uint32_t)(millis() - flashHeldSince) >=
               PLANTOWER_FLASH_HOLD_MS) {
      forgetWifiAndReboot("FLASH button held");
    }
  } else {
    flashHeldSince = 0;
  }
}

static void handleWifiFallback(void) {
  if (wifiConnector.isConnected()) {
    wifiLostSince = 0;
    return;
  }
  if (!wifiConnector.hasConfigurated()) {
    return;
  }
  if (wifiLostSince == 0) {
    wifiLostSince = millis();
    return;
  }
  if ((uint32_t)(millis() - wifiLostSince) >= PLANTOWER_WIFI_FALLBACK_MS) {
    forgetWifiAndReboot("saved Wi-Fi unreachable");
  }
}

static void wifiFactoryConfigure(void) {
  WiFi.persistent(true);
  WiFi.begin("airgradient", "cleanair");
  WiFi.persistent(false);
  oledDisplay.setText("Configure WiFi", "connect to", "\'airgradient\'");
  delay(2500);
  oledDisplay.setText("Rebooting...", "", "");
  delay(2500);
  oledDisplay.setText("", "", "");
  ESP.restart();
}

static void mqttHandle(void) {
  if(mqttClient.isConnected() == false) {
    mqttClient.connect(String("airgradient-") + ag.deviceId());
  }

  if (mqttClient.isConnected()) {
    String payload = measurements.toString(true, fwMode, wifiConnector.RSSI());
    String topic = "airgradient/readings/" + ag.deviceId();
    if (mqttClient.publish(topic.c_str(), payload.c_str(), payload.length())) {
      Serial.println("MQTT sync success");
    } else {
      Serial.println("MQTT sync failure");
    }
  }
}

static void sendDataToAg() {
  /** Change oledDisplay and led state */
  stateMachine.displayHandle(AgStateMachineWiFiOkServerConnecting);

  delay(1500);
  if (apiClient.sendPing(wifiConnector.RSSI(), measurements.bootCount())) {
    stateMachine.displayHandle(AgStateMachineWiFiOkServerConnected);
  } else {
    stateMachine.displayHandle(AgStateMachineWiFiOkServerConnectFailed);
  }
  delay(DISPLAY_DELAY_SHOW_CONTENT_MS);
}

void dispSensorNotFound(String ss) {
  oledDisplay.setText("Sensor", ss.c_str(), "not found");
  delay(2000);
}

static void boardInit(void) {
  /** Display init */
  oledDisplay.begin();

  /** Show boot display */
  Serial.println("Firmware Version: " + ag.getVersion());

  if (ag.isBasic()) {
    oledDisplay.setText("DIY Basic", ag.getVersion().c_str(), "");
  } else {
    oledDisplay.setText("AirGradient ONE",
                        "FW Version: ", ag.getVersion().c_str());
  }

  delay(DISPLAY_DELAY_SHOW_CONTENT_MS);

  ag.watchdog.begin();

  /** Show message init sensor */
  oledDisplay.setText("Sensor", "init...", "");

  /** Init sensor SGP41 */
  configuration.hasSensorSGP = false;
  // if (sgp41Init() == false) {
  //   dispSensorNotFound("SGP41");
  // }

  /** No onboard SHT. Keep the SHT flag so AirGradient still accepts rhum/atmp
   *  from Open-Meteo until the DHT22 / AM2302 is wired. */
  configuration.hasSensorSHT = true;

  /** Plantower build: no S8 CO2 sensor installed. */
  configuration.hasSensorS8 = false;

  /** Init PMS5003 */
  configuration.hasSensorPMS1 = true;
  configuration.hasSensorPMS2 = false;
  if (ag.pms5003.begin(&Serial) == false) {
    Serial.println("PMS sensor not found");
    configuration.hasSensorPMS1 = false;
    pmsRetryAt = millis() + 10000;

    dispSensorNotFound("PMS");
  }

  /** Set S8 CO2 abc days period */
  if (configuration.hasSensorS8) {
    if (ag.s8.setAbcPeriod(configuration.getCO2CalibrationAbcDays() * 24)) {
      Serial.println("Set S8 AbcDays successful");
    } else {
      Serial.println("Set S8 AbcDays failure");
    }
  }

  localServer.setFwMode(fwMode);
}

static void failedHandler(String msg) {
  while (true) {
    Serial.println(msg);
    delay(1000);
  }
}

static void configurationUpdateSchedule(void) {
  if (configuration.isOfflineMode() ||
      configuration.getConfigurationControl() == ConfigurationControl::ConfigurationControlLocal) {
    Serial.println("Ignore fetch server configuration. Either mode is offline "
                   "or configurationControl set to local");
    apiClient.resetFetchConfigurationStatus();
    return;
  }

  if (apiClient.fetchServerConfiguration()) {
    configUpdateHandle();
  }
}

static void configUpdateHandle() {
  if (configuration.isUpdated() == false) {
    return;
  }

  applyPlantowerLocalSettings(false);

  stateMachine.executeCo2Calibration();

  String mqttUri = configuration.getMqttBrokerUri();
  if (mqttClient.isCurrentUri(mqttUri) == false) {
    mqttClient.end();
    initMqtt();
  }

  if (configuration.hasSensorSGP) {
    if (configuration.noxLearnOffsetChanged() ||
        configuration.tvocLearnOffsetChanged()) {
      ag.sgp41.end();

      int oldTvocOffset = ag.sgp41.getTvocLearningOffset();
      int oldNoxOffset = ag.sgp41.getNoxLearningOffset();
      bool result = sgp41Init();
      const char *resultStr = "successful";
      if (!result) {
        resultStr = "failure";
      }
      if (oldTvocOffset != configuration.getTvocLearningOffset()) {
        Serial.printf("Setting tvocLearningOffset from %d to %d hours %s\r\n",
                      oldTvocOffset, configuration.getTvocLearningOffset(),
                      resultStr);
      }
      if (oldNoxOffset != configuration.getNoxLearningOffset()) {
        Serial.printf("Setting noxLearningOffset from %d to %d hours %s\r\n",
                      oldNoxOffset, configuration.getNoxLearningOffset(),
                      resultStr);
      }
    }
  }

  if (configuration.isDisplayBrightnessChanged()) {
    oledDisplay.setBrightness(configuration.getDisplayBrightness());
  }

  appDispHandler();
}

static void appDispHandler(void) {
  AgStateMachineState state = AgStateMachineNormal;

  /** Only show display status on online mode. */
  if (configuration.isOfflineMode() == false) {
    if (wifiConnector.isConnected() == false) {
      state = AgStateMachineWiFiLost;
    } else if (apiClient.isFetchConfigurationFailed()) {
      state = AgStateMachineSensorConfigFailed;
      if (apiClient.isNotAvailableOnDashboard()) {
        stateMachine.displaySetAddToDashBoard();
      } else {
        stateMachine.displayClearAddToDashBoard();
      }
    } else if (apiClient.isPostToServerFailed()) {
      state = AgStateMachineServerLost;
    }
  }
  stateMachine.displayHandle(state);
}

static void oledDisplaySchedule(void) {

  appDispHandler();
}

static void updateTvoc(void) {
  if (!configuration.hasSensorSGP) {
    return;
  }

  measurements.update(Measurements::TVOC, ag.sgp41.getTvocIndex());
  measurements.update(Measurements::TVOCRaw, ag.sgp41.getTvocRaw());
  measurements.update(Measurements::NOx, ag.sgp41.getNoxIndex());
  measurements.update(Measurements::NOxRaw, ag.sgp41.getNoxRaw());
}

static void updatePm(void) {
  if (ag.pms5003.connected()) {
    measurements.update(Measurements::PM01, ag.pms5003.getPm01Ae());
    measurements.update(Measurements::PM25, ag.pms5003.getPm25Ae());
    measurements.update(Measurements::PM10, ag.pms5003.getPm10Ae());
    measurements.update(Measurements::PM03_PC, ag.pms5003.getPm03ParticleCount());
  } else {
    measurements.update(Measurements::PM01, utils::getInvalidPmValue());
    measurements.update(Measurements::PM25, utils::getInvalidPmValue());
    measurements.update(Measurements::PM10, utils::getInvalidPmValue());
    measurements.update(Measurements::PM03_PC, utils::getInvalidPmValue());
  }
  uint32_t now = millis();
  localServer.setPlantowerReadingTiming(now, now + activePmUpdateInterval);
}

static void applyPlantowerLocalSettings(bool force) {
  int uploadMs = configuration.getPlantowerUploadIntervalMs();
  int pmMs = configuration.getPlantowerPmReadIntervalMs();
  int warmupMs = configuration.getPlantowerWarmupMs();
  bool dutyMode = configuration.isPlantowerDutyCycleEnabled();

  if (!force && uploadMs == activeServerSyncInterval &&
      pmMs == activePmUpdateInterval &&
      warmupMs == activePmsWarmupInterval &&
      dutyMode == activePlantowerDutyMode) {
    return;
  }

  activeServerSyncInterval = uploadMs;
  activePmUpdateInterval = pmMs;
  activePmsWarmupInterval = warmupMs;
  activePlantowerDutyMode = dutyMode;

  agApiPostSchedule.setPeriod(activeServerSyncInterval);
  agApiPostSchedule.update();
  pmsSchedule.setPeriod(activePmUpdateInterval);
  pmsSchedule.update();
  setMeasurementMaxPeriod();

  Serial.printf("Plantower local settings: upload=%ds pm_cycle=%ds warmup=%ds mode=%s\n",
                activeServerSyncInterval / 1000,
                activePmUpdateInterval / 1000,
                activePmsWarmupInterval / 1000,
                activePlantowerDutyMode ? "duty" : "continuous");

  if (!activePlantowerDutyMode && (pmsDutySleeping || pmsDutyWarming)) {
    ag.pms5003.wake();
    pmsDutySleeping = false;
    pmsDutyWarming = false;
    pmsDutyWakeStarted = 0;
  }
}

static void handlePlantowerSensor(void) {
  if (!configuration.hasSensorPMS1) {
    uint32_t now = millis();
    if (pmsRetryAt == 0 || (int32_t)(now - pmsRetryAt) >= 0) {
      Serial.println("Retrying PMS sensor init");
      if (ag.pms5003.begin(&Serial)) {
        Serial.println("PMS sensor recovered");
        configuration.hasSensorPMS1 = true;
        pmsDutySleeping = false;
        pmsDutyWarming = false;
        pmsDutyWakeStarted = 0;
        pmsDutyLastRead = 0;
      } else {
        pmsRetryAt = millis() + 60000;
      }
    }
    return;
  }

  if (!activePlantowerDutyMode) {
    pmsSchedule.run();
    ag.pms5003.handle();
    return;
  }

  uint32_t now = millis();
  if (!pmsDutySleeping && !pmsDutyWarming) {
    ag.pms5003.handle();
    updatePm();
    ag.pms5003.sleep();
    pmsDutySleeping = true;
    pmsDutyLastRead = now;
    return;
  }

  if (pmsDutySleeping) {
    uint32_t sleepMs = activePmUpdateInterval > activePmsWarmupInterval
                           ? activePmUpdateInterval - activePmsWarmupInterval
                           : 0;
    if ((uint32_t)(now - pmsDutyLastRead) >= sleepMs) {
      ag.pms5003.wake();
      pmsDutySleeping = false;
      pmsDutyWarming = true;
      pmsDutyWakeStarted = now;
    }
    return;
  }

  ag.pms5003.handle();
  if ((uint32_t)(now - pmsDutyWakeStarted) >=
      (uint32_t)activePmsWarmupInterval) {
    updatePm();
    ag.pms5003.sleep();
    pmsDutyWarming = false;
    pmsDutySleeping = true;
    pmsDutyLastRead = now;
  }
}

static void sendDataToServer(void) {
  /** Increment bootcount when send measurements data is scheduled */
  int bootCount = measurements.bootCount() + 1;
  measurements.setBootCount(bootCount);

  if (configuration.isOfflineMode() || !configuration.isPostDataToAirGradient()) {
    Serial.println("Skipping transmission of data to AG server. Either mode is offline "
                   "or post data to server disabled");
    return;
  }

  if (wifiConnector.isConnected() == false) {
    Serial.println("WiFi not connected, skipping data transmission to AG server");
    return;
  }

  String syncData = measurements.toString(false, fwMode, wifiConnector.RSSI());
  if (apiClient.postToServer(syncData)) {
    Serial.println();
    Serial.println("Online mode and isPostToAirGradient = true");
    Serial.println();
  }
}

static bool parseOpenMeteoCurrent(const String &body, float &temp,
                                  float &rhum) {
  int currentIdx = body.indexOf("\"current\":{");
  if (currentIdx < 0) {
    currentIdx = body.indexOf("\"current\": {");
  }
  if (currentIdx < 0) {
    return false;
  }

  String current = body.substring(currentIdx);
  int tempKey = current.indexOf("\"temperature_2m\"");
  int rhumKey = current.indexOf("\"relative_humidity_2m\"");
  if (tempKey < 0 || rhumKey < 0) {
    return false;
  }

  int tempColon = current.indexOf(':', tempKey);
  int rhumColon = current.indexOf(':', rhumKey);
  if (tempColon < 0 || rhumColon < 0) {
    return false;
  }

  temp = current.substring(tempColon + 1).toFloat();
  rhum = current.substring(rhumColon + 1).toFloat();
  return utils::isValidTemperature(temp) && utils::isValidHumidity(rhum);
}

static bool openMeteoLocationIsSet(void) {
  return !(openMeteoLat == 0.0f && openMeteoLon == 0.0f);
}

static bool fetchOpenMeteo(void) {
  if (!openMeteoLocationIsSet()) {
    Serial.println("Open-Meteo skipped: set latitude/longitude on the Network tab");
    return false;
  }
  if (!wifiConnector.isConnected()) {
    Serial.println("Open-Meteo skipped: Wi-Fi not connected");
    return false;
  }

  String uri = String("http://api.open-meteo.com/v1/forecast?latitude=") +
               String(openMeteoLat, 4) + "&longitude=" +
               String(openMeteoLon, 4) +
               "&current=temperature_2m,relative_humidity_2m";

  WiFiClient wifiClient;
  HTTPClient client;
  client.setTimeout(8000);
  if (!client.begin(wifiClient, uri)) {
    Serial.println("Open-Meteo begin failed");
    return false;
  }

  int code = client.GET();
  String body = (code == 200) ? client.getString() : String();
  client.end();
  Serial.printf("Open-Meteo GET code=%d bytes=%d lat=%.4f lon=%.4f\n", code,
                body.length(), openMeteoLat, openMeteoLon);
  if (code != 200) {
    return false;
  }

  float temp = utils::getInvalidTemperature();
  float rhum = utils::getInvalidHumidity();
  if (!parseOpenMeteoCurrent(body, temp, rhum)) {
    Serial.println("Open-Meteo parse failed");
    return false;
  }

  measurements.update(Measurements::Temperature, temp);
  measurements.update(Measurements::Humidity, rhum);
  Serial.printf("Open-Meteo API atmp=%.1f rhum=%.0f\n", temp, rhum);
  return true;
}

static void tempHumUpdate(void) {
  if (!fetchOpenMeteo()) {
    if (!utils::isValidTemperature(
            measurements.getFloat(Measurements::Temperature)) ||
        !utils::isValidHumidity(
            measurements.getFloat(Measurements::Humidity))) {
      measurements.update(Measurements::Temperature,
                          utils::getInvalidTemperature());
      measurements.update(Measurements::Humidity, utils::getInvalidHumidity());
    }
  }
}

static void loadOpenMeteoLocation(void) {
  EEPROM.begin(32);
  uint32_t magic = 0;
  float lat = 0;
  float lon = 0;
  EEPROM.get(PLANTOWER_LOCATION_EEPROM_ADDR, magic);
  EEPROM.get(PLANTOWER_LOCATION_EEPROM_ADDR + 4, lat);
  EEPROM.get(PLANTOWER_LOCATION_EEPROM_ADDR + 8, lon);
  if (magic == PLANTOWER_LOCATION_MAGIC && lat >= -90.0f && lat <= 90.0f &&
      lon >= -180.0f && lon <= 180.0f &&
      !(lat == 0.0f && lon == 0.0f)) {
    openMeteoLat = lat;
    openMeteoLon = lon;
  } else {
    openMeteoLat = PLANTOWER_OPEN_METEO_LAT;
    openMeteoLon = PLANTOWER_OPEN_METEO_LON;
  }
  Serial.printf("Open-Meteo location lat=%.4f lon=%.4f\n", openMeteoLat,
                openMeteoLon);
}

static void saveOpenMeteoLocation(float lat, float lon) {
  if (lat < -90.0f || lat > 90.0f || lon < -180.0f || lon > 180.0f) {
    return;
  }
  openMeteoLat = lat;
  openMeteoLon = lon;
  EEPROM.begin(32);
  uint32_t magic = PLANTOWER_LOCATION_MAGIC;
  EEPROM.put(PLANTOWER_LOCATION_EEPROM_ADDR, magic);
  EEPROM.put(PLANTOWER_LOCATION_EEPROM_ADDR + 4, lat);
  EEPROM.put(PLANTOWER_LOCATION_EEPROM_ADDR + 8, lon);
  EEPROM.commit();
  Serial.printf("Open-Meteo location saved lat=%.4f lon=%.4f\n", lat, lon);
  tempHumUpdate();
}

/* Set max period for each measurement type based on sensor update interval*/
void setMeasurementMaxPeriod() {
  /// Max period for S8 sensors measurements
  measurements.maxPeriod(Measurements::CO2, calculateMaxPeriod(SENSOR_CO2_UPDATE_INTERVAL));
  /// Max period for SGP sensors measurements
  measurements.maxPeriod(Measurements::TVOC, calculateMaxPeriod(SENSOR_TVOC_UPDATE_INTERVAL));
  measurements.maxPeriod(Measurements::TVOCRaw, calculateMaxPeriod(SENSOR_TVOC_UPDATE_INTERVAL));
  measurements.maxPeriod(Measurements::NOx, calculateMaxPeriod(SENSOR_TVOC_UPDATE_INTERVAL));
  measurements.maxPeriod(Measurements::NOxRaw, calculateMaxPeriod(SENSOR_TVOC_UPDATE_INTERVAL));
  /// Max period for PMS sensors measurements
  measurements.maxPeriod(Measurements::PM25, calculateMaxPeriod(activePmUpdateInterval));
  measurements.maxPeriod(Measurements::PM01, calculateMaxPeriod(activePmUpdateInterval));
  measurements.maxPeriod(Measurements::PM10, calculateMaxPeriod(activePmUpdateInterval));
  measurements.maxPeriod(Measurements::PM03_PC, calculateMaxPeriod(activePmUpdateInterval));
  // Open-Meteo supplies one reading per fetch. Keep the period at 1 so a
  // single valid sample is enough for the next AirGradient upload.
  measurements.maxPeriod(Measurements::Temperature, 1);
  measurements.maxPeriod(Measurements::Humidity, 1);
}

int calculateMaxPeriod(int updateInterval) {
  // 0.5 is 50% reduced interval for max period
  int maxPeriod = (activeServerSyncInterval - (activeServerSyncInterval * 0.5)) / updateInterval;
  if (maxPeriod < 1) {
    return 1;
  }
  return maxPeriod;
}
