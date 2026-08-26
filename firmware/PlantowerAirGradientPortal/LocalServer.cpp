#include "LocalServer.h"
#include <ESP8266WiFi.h>

static void appendOption(String &html, int value, int selectedValue,
                         const char *label) {
  html += "<option value='";
  html += String(value);
  html += "'";
  if (value == selectedValue) {
    html += " selected";
  }
  html += ">";
  html += label;
  html += "</option>";
}

LocalServer::LocalServer(Stream &log, OpenMetrics &openMetrics,
                         Measurements &measure, Configuration &config,
                         WifiConnector &wifiConnector)
    : PrintLog(log, "LocalServer"), openMetrics(openMetrics), measure(measure),
      config(config), wifiConnector(wifiConnector), server(80) {}

LocalServer::~LocalServer() {}

bool LocalServer::begin(void) {
  server.on("/measures/current", HTTP_GET, [this]() { _GET_measure(); });
  server.on(openMetrics.getApi(), HTTP_GET, [this]() { _GET_metrics(); });
  server.on("/config", HTTP_GET, [this]() { _GET_config(); });
  server.on("/config", HTTP_PUT, [this]() { _PUT_config(); });
  server.on("/", HTTP_GET, [this]() { _GET_home(); });
  server.on("/settings", HTTP_GET, [this]() { _GET_home(); });
  server.on("/network", HTTP_GET, [this]() { _GET_settings(); });
  server.on("/help", HTTP_GET, [this]() { _GET_help(); });
  server.on("/plantower/settings", HTTP_GET,
            [this]() { _GET_plantower_settings(); });
  server.on("/plantower/settings", HTTP_POST,
            [this]() { _POST_plantower_settings(); });
  server.on("/wifi", HTTP_POST, [this]() { _POST_wifi(); });
  server.on("/wifi/setup", HTTP_POST, [this]() { _POST_wifi_setup(); });
  server.on("/location", HTTP_POST, [this]() { _POST_location(); });
  server.begin();
  logInfo("Init: " + getHostname() + ".local");
  logInfo("IP: " + wifiConnector.localIpStr() +
          " RSSI: " + String(wifiConnector.RSSI()) + " dBm");
  return true;
}

void LocalServer::setAirGraident(AirGradient *ag) { this->ag = ag; }

String LocalServer::getHostname(void) {
  return "airgradient_" + ag->deviceId();
}

void LocalServer::setPlantowerReadingTiming(uint32_t lastReadingMs,
                                            uint32_t nextReadingMs) {
  plantowerLastReadingMs = lastReadingMs;
  plantowerNextReadingMs = nextReadingMs;
}

void LocalServer::setOpenMeteoLocation(float lat, float lon,
                                       void (*saveFn)(float, float)) {
  openMeteoLat = lat;
  openMeteoLon = lon;
  saveOpenMeteoLocation = saveFn;
}

String LocalServer::formatApiTemp(float value) {
  if (!utils::isValidTemperature(value)) {
    return String("--");
  }
  String formatted = String(value, 1);
  formatted += " C";
  return formatted;
}

String LocalServer::formatApiRhum(float value) {
  if (!utils::isValidHumidity(value)) {
    return String("--");
  }
  return String(value, 0) + "%";
}

void LocalServer::_handle(void) { server.handleClient(); }

void LocalServer::_GET_config(void) {
  if (ag->isOne()) {
    server.send(200, "application/json", config.toString());
  } else {
    server.send(200, "application/json", config.toString(fwMode));
  }
}

void LocalServer::_PUT_config(void) {
  String data = server.arg(0);
  String response = "";
  int statusCode = 400;
  if (config.parse(data, true)) {
    statusCode = 200;
    response = "Success";
  } else {
    response = config.getFailedMesage();
  }
  server.send(statusCode, "text/plain", response);
}

void LocalServer::_GET_metrics(void) {
  server.send(200, openMetrics.getApiContentType(), openMetrics.getPayload());
}

void LocalServer::_GET_measure(void) {
  String toSend = measure.toString(true, fwMode, wifiConnector.RSSI());
  server.send(200, "application/json", toSend);
}

String LocalServer::wifiSignalLabel(int rssi) {
  if (rssi >= -60) {
    return String("strong");
  }
  if (rssi >= -70) {
    return String("okay");
  }
  if (rssi >= -80) {
    return String("weak");
  }
  return String("very weak");
}

String LocalServer::formatPmValue(int value, bool particleCount) {
  if (!utils::isValidPm(value)) {
    return String("--");
  }
  String formatted = String(value);
  if (!particleCount) {
    formatted += " ug/m3";
  }
  return formatted;
}

String LocalServer::currentSsid(void) {
  String ssid = WiFi.SSID();
  if (ssid.length() == 0) {
    return String("not set");
  }
  return ssid;
}

void LocalServer::sendHtml(const String &html) {
  server.send(200, "text/html", html);
}

void LocalServer::rebootAfterReply(const String &message) {
  server.send(200, "text/html", message);
  server.client().stop();
  delay(800);
  ESP.restart();
}

String LocalServer::pageShell(const char *activeTab, const String &body) {
  String html;
  html.reserve(7800);
  html += F("<!doctype html><html lang='en'><head><meta charset='utf-8'>");
  html += F("<meta name='viewport' content='width=device-width,initial-scale=1'>");
  html += F("<title>Air sensor</title><style>");
  html += F("body{margin:0;background:#eef3f1;color:#173038;font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif}");
  html += F("header{background:#0f766e;color:white;padding:18px 20px 14px}");
  html += F("header h1{margin:0;font-size:22px}header p{margin:6px 0 0;opacity:.9}");
  html += F("nav{display:flex;gap:8px;padding:12px 16px 0}");
  html += F("nav a{flex:1;text-align:center;text-decoration:none;color:#0f766e;background:#fff;border-radius:999px;padding:10px 8px;font-weight:650;border:1px solid #cfe4df}");
  html += F("nav a.active{background:#0f766e;color:#fff;border-color:#0f766e}");
  html += F("main{max-width:640px;margin:0 auto;padding:16px}");
  html += F(".card{background:#fff;border:1px solid #d5e4e0;border-radius:14px;padding:16px;margin:0 0 14px}");
  html += F(".hero{text-align:center;padding:22px 12px}.hero b{display:block;font-size:48px;line-height:1.1;margin:6px 0}");
  html += F(".grid{display:grid;grid-template-columns:1fr 1fr;gap:10px}");
  html += F(".stat span{display:block;color:#5b7278;font-size:13px}.stat b{font-size:20px}");
  html += F("label{display:block;font-weight:650;margin:16px 0 6px}");
  html += F("select,input{width:100%;box-sizing:border-box;font-size:16px;padding:11px;border:1px solid #bdd0cb;border-radius:10px}");
  html += F(".choice{display:flex;gap:10px;margin:8px 0;align-items:center}.choice input{width:auto}");
  html += F("button,.btn{display:block;width:100%;box-sizing:border-box;margin-top:16px;background:#0f766e;color:#fff;border:0;border-radius:10px;padding:12px;font-size:16px;font-weight:700}");
  html += F("button.secondary{background:#fff;color:#0f766e;border:1px solid #0f766e}");
  html += F("button.danger{background:#b42318}");
  html += F(".ok{background:#e8f6ee;border:1px solid #a8dec0;color:#174f2d;border-radius:10px;padding:10px;margin:0 0 12px}");
  html += F(".warn{background:#fff6e5;border:1px solid #f0d59a;color:#7a4b00;border-radius:10px;padding:10px;margin:0 0 12px}");
  html += F("p,li{line-height:1.5;color:#3d555b}ol{padding-left:20px}");
  html += F(".meta{font-size:13px;color:#5b7278}code{background:#eef3f1;padding:1px 5px;border-radius:4px}");
  html += F("@media(max-width:560px){.grid{grid-template-columns:1fr}.hero b{font-size:40px}}");
  html += F("</style></head><body><header><h1>Outdoor PM sensor</h1><p>Plantower PMS5003 to AirGradient</p></header><nav>");
  html += F("<a href='/' class='");
  html += (strcmp(activeTab, "readings") == 0) ? "active" : "";
  html += F("'>Readings</a><a href='/network' class='");
  html += (strcmp(activeTab, "network") == 0) ? "active" : "";
  html += F("'>Network</a><a href='/help' class='");
  html += (strcmp(activeTab, "help") == 0) ? "active" : "";
  html += F("'>Help</a></nav><main>");
  html += body;
  html += F("</main></body></html>");
  return html;
}

void LocalServer::_GET_home(void) {
  int rssi = wifiConnector.RSSI();
  int pm25 = measure.get(Measurements::PM25);
  int pm01 = measure.get(Measurements::PM01);
  int pm10 = measure.get(Measurements::PM10);
  int pm003 = measure.get(Measurements::PM03_PC);
  float atmp = measure.getFloat(Measurements::Temperature);
  float rhum = measure.getFloat(Measurements::Humidity);
  uint32_t now = millis();
  bool hasReading = plantowerLastReadingMs != 0;
  uint32_t ageSec =
      hasReading ? (uint32_t)(now - plantowerLastReadingMs) / 1000 : 0;
  uint32_t nextSec = 0;
  if (hasReading && plantowerNextReadingMs != 0 &&
      (int32_t)(plantowerNextReadingMs - now) > 0) {
    nextSec = (uint32_t)(plantowerNextReadingMs - now) / 1000;
  }

  String body;
  body += F("<div class='card hero'><span>PM2.5</span><b>");
  body += formatPmValue(pm25);
  body += F("</b><span class='meta'>Tiny particles that affect breathing</span></div>");
  body += F("<div class='card grid'><div class='stat'><span>PM1</span><b>");
  body += formatPmValue(pm01);
  body += F("</b></div><div class='stat'><span>PM10</span><b>");
  body += formatPmValue(pm10);
  body += F("</b></div><div class='stat'><span>PM0.3 count</span><b>");
  body += formatPmValue(pm003, true);
  body += F("</b></div><div class='stat'><span>Last reading</span><b>");
  body += hasReading ? String(ageSec) + "s ago" : String("waiting");
  body += F("</b></div><div class='stat'><span>Next reading</span><b id='nextRead' data-seconds='");
  body += String(nextSec);
  body += "'>";
  body += hasReading ? String(nextSec) + "s" : String("waiting");
  body += F("</b></div><div class='stat'><span>AirGradient</span><b>");
  body += wifiConnector.isConnected() ? "online" : "offline";
  body += F("</b></div></div>");
  body += F("<div class='card grid'><div class='stat'><span>Humidity</span><b>");
  body += formatApiRhum(rhum);
  body += F("</b><span>API: Open-Meteo</span></div><div class='stat'><span>Temperature</span><b>");
  body += formatApiTemp(atmp);
  body += F("</b><span>API: Open-Meteo</span></div></div>");
  body += F("<div class='card grid'><div class='stat'><span>Wi-Fi</span><b>");
  body += currentSsid();
  body += F("</b><span>");
  body += wifiSignalLabel(rssi);
  body += " · ";
  body += String(rssi);
  body += F(" dBm</span></div><div class='stat'><span>Local address</span><b>");
  body += wifiConnector.localIpStr();
  body += F("</b><span>");
  body += getHostname();
  body += F(".local</span></div></div>");
  if (openMeteoLat == 0.0f && openMeteoLon == 0.0f) {
    body += F("<div class='warn'>Set your weather location on the Network tab so humidity can be sent to AirGradient.</div>");
  }
  body += F("<p class='meta'>PM values are raw Plantower readings. Humidity and temperature come from Open-Meteo for the saved location until an onboard humidity sensor is wired. AirGradient uses that humidity for EPA correction on the public map.</p>");
  body += F("<script>let n=document.getElementById('nextRead');if(n){let s=parseInt(n.dataset.seconds||'0',10);setInterval(()=>{if(s>0)s--;n.textContent=s+'s';},1000);}</script>");
  sendHtml(pageShell("readings", body));
}

void LocalServer::_GET_settings(void) {
  int uploadSec = config.getPlantowerUploadIntervalSec();
  int pmReadSec = config.getPlantowerPmReadIntervalSec();
  int warmupSec = config.getPlantowerWarmupSec();
  bool duty = config.isPlantowerDutyCycleEnabled();
  int rssi = wifiConnector.RSSI();

  String body;
  if (server.hasArg("saved")) {
    body += F("<div class='ok'>Saved. The board applies this without reflashing.</div>");
  }
  body += F("<div class='card'><h2 style='margin:0 0 8px;font-size:18px'>Home Wi-Fi</h2>");
  body += F("<p>The firmware does not contain your password. Changing network here saves it on the board and reboots.</p>");
  body += F("<p><b>Now using:</b> ");
  body += currentSsid();
  body += F("</p><p class='meta'>");
  body += wifiConnector.isConnected() ? "Connected" : "Not connected";
  body += " · ";
  body += String(rssi);
  body += F(" dBm · ");
  body += wifiConnector.localIpStr();
  body += F("</p><form method='post' action='/wifi'>");
  body += F("<label for='ssid'>Network name (SSID)</label>");
  body += F("<input id='ssid' name='ssid' autocomplete='off' required placeholder='Home Wi-Fi name'>");
  body += F("<label for='password'>Password</label>");
  body += F("<input id='password' name='password' type='password' autocomplete='new-password'>");
  body += F("<button type='submit'>Save Wi-Fi and reboot</button></form>");
  body += F("<form method='post' action='/wifi/setup' onsubmit=\"return confirm('This forgets the current Wi-Fi and opens the setup hotspot.');\">");
  body += F("<button class='secondary' type='submit'>Forget Wi-Fi and open setup hotspot</button></form>");
  body += F("<p class='meta'>If the board cannot join after a router change, join hotspot <code>airgradient-");
  body += ag->deviceId();
  body += F("</code> with password <code>cleanair</code>, then open <code>http://192.168.4.1</code>.</p></div>");

  body += F("<div class='card'><h2 style='margin:0 0 8px;font-size:18px'>Weather location</h2>");
  body += F("<p>Open-Meteo needs a nearby latitude and longitude so humidity is close to this sensor. Use your phone GPS, Google Maps, or <a href='https://www.openstreetmap.org/'>OpenStreetMap</a>. Four decimal places is enough.</p>");
  if (openMeteoLat == 0.0f && openMeteoLon == 0.0f) {
    body += F("<div class='warn'>Location not set. AirGradient EPA correction needs humidity, so save coordinates before leaving this page.</div>");
  } else {
    body += F("<p class='meta'>Now using ");
    body += String(openMeteoLat, 4);
    body += ", ";
    body += String(openMeteoLon, 4);
    body += F("</p>");
  }
  body += F("<form method='post' action='/location'>");
  body += F("<label for='lat'>Latitude</label>");
  body += F("<input id='lat' name='lat' inputmode='decimal' required placeholder='e.g. 51.5074' value='");
  if (!(openMeteoLat == 0.0f && openMeteoLon == 0.0f)) {
    body += String(openMeteoLat, 4);
  }
  body += F("'>");
  body += F("<label for='lon'>Longitude</label>");
  body += F("<input id='lon' name='lon' inputmode='decimal' required placeholder='e.g. -0.1278' value='");
  if (!(openMeteoLat == 0.0f && openMeteoLon == 0.0f)) {
    body += String(openMeteoLon, 4);
  }
  body += F("'>");
  body += F("<button type='submit'>Save location</button></form>");
  body += F("<p class='meta'>This is a weather-model point for your outdoor location, not a humidity chip on the board.</p></div>");

  body += F("<div class='card'><h2 style='margin:0 0 8px;font-size:18px'>Sensor settings</h2>");
  body += F("<form method='post' action='/plantower/settings'>");
  body += F("<label for='uploadSec'>Send to AirGradient every</label><select id='uploadSec' name='uploadSec'>");
  appendOption(body, 60, uploadSec, "1 minute");
  appendOption(body, 300, uploadSec, "5 minutes");
  appendOption(body, 600, uploadSec, "10 minutes");
  appendOption(body, 1800, uploadSec, "30 minutes");
  appendOption(body, 3600, uploadSec, "60 minutes");
  body += F("</select>");
  body += F("<label for='pmReadSec'>Take a PM reading every</label><select id='pmReadSec' name='pmReadSec'>");
  appendOption(body, 2, pmReadSec, "2 seconds");
  appendOption(body, 60, pmReadSec, "1 minute");
  appendOption(body, 300, pmReadSec, "5 minutes");
  appendOption(body, 600, pmReadSec, "10 minutes");
  appendOption(body, 1800, pmReadSec, "30 minutes");
  body += F("</select>");
  body += F("<details style='margin-top:16px'><summary>Advanced sensor mode</summary>");
  body += F("<label>Plantower mode</label><div class='choice'><input id='modeContinuous' type='radio' name='mode' value='continuous'");
  if (!duty) {
    body += F(" checked");
  }
  body += F("><label for='modeContinuous'>Continuous fan and laser</label></div>");
  body += F("<div class='choice'><input id='modeDuty' type='radio' name='mode' value='duty'");
  if (duty) {
    body += F(" checked");
  }
  body += F("><label for='modeDuty'>Sleep between readings</label></div>");
  body += F("<label for='warmupSec'>Warmup before a sleeping reading</label><select id='warmupSec' name='warmupSec'>");
  appendOption(body, 30, warmupSec, "30 seconds");
  appendOption(body, 60, warmupSec, "60 seconds");
  appendOption(body, 90, warmupSec, "90 seconds");
  appendOption(body, 120, warmupSec, "120 seconds");
  body += F("</select><p class='meta'>Sleep mode uses the PMS5003 serial sleep command. If readings look jumpy, use a longer warmup.</p></details>");
  body += F("<button type='submit'>Save sensor settings</button></form></div>");
  sendHtml(pageShell("network", body));
}

void LocalServer::_GET_help(void) {
  String body;
  body += F("<div class='card'><h2 style='margin:0 0 8px;font-size:18px'>First setup</h2><ol>");
  body += F("<li>Flash the firmware once over USB. After that, you do not reflash to change Wi-Fi.</li>");
  body += F("<li>Power the board from USB.</li>");
  body += F("<li>On your phone, join the hotspot named <code>airgradient-");
  body += ag->deviceId();
  body += F("</code>. Password is <code>cleanair</code>.</li>");
  body += F("<li>Open <code>http://192.168.4.1</code> if a page does not appear by itself.</li>");
  body += F("<li>Enter your home Wi-Fi name and password. The board saves them and joins that network.</li>");
  body += F("<li>Register serial <code>");
  body += ag->deviceId();
  body += F("</code> in the AirGradient dashboard as a DIY outdoor monitor.</li></ol></div>");
  body += F("<div class='card'><h2 style='margin:0 0 8px;font-size:18px'>If you change routers</h2>");
  body += F("<p>If this page still opens, use the Network tab and save the new Wi-Fi. No reflash.</p>");
  body += F("<p>If the board cannot join the new network, the hotspot comes back. Join it from your phone and enter the new details.</p>");
  body += F("<p>You can also hold the NodeMCU <b>FLASH</b> button for 3 seconds to forget Wi-Fi and open the hotspot.</p></div>");
  body += F("<div class='card'><h2 style='margin:0 0 8px;font-size:18px'>Wiring</h2>");
  body += F("<p>PMS5003 VCC to 5V / VBUS. GND to GND. TX to D5 (GPIO14). RX to D6 (GPIO12).</p>");
  body += F("<p class='meta'>USB is only for power and flashing. The setup page is always over Wi-Fi, not USB.</p></div>");
  body += F("<div class='card'><h2 style='margin:0 0 8px;font-size:18px'>Humidity and temperature</h2>");
  body += F("<p>There is no onboard humidity chip yet. The board asks Open-Meteo for nearby temperature and relative humidity, then uploads those as <code>atmp</code> and <code>rhum</code> so AirGradient can apply EPA correction.</p>");
  body += F("<p>Set the location on the Network tab. Find coordinates at <a href='https://www.openstreetmap.org/'>openstreetmap.org</a> or from your phone. Use the outdoor sensor position, not a city-centre default if you live elsewhere.</p></div>");
  body += F("<div class='card'><h2 style='margin:0 0 8px;font-size:18px'>What the numbers mean</h2>");
  body += F("<p>PM2.5 is the main outdoor smoke and haze number. Lower is cleaner. This page shows raw Plantower values. Map, sharing, and corrections stay in AirGradient.</p></div>");
  sendHtml(pageShell("help", body));
}

void LocalServer::_GET_plantower_settings(void) {
  String data = "{";
  int rssi = wifiConnector.RSSI();
  uint32_t now = millis();
  bool hasReading = plantowerLastReadingMs != 0;
  uint32_t ageSec =
      hasReading ? (uint32_t)(now - plantowerLastReadingMs) / 1000 : 0;
  uint32_t nextSec = 0;
  if (hasReading && plantowerNextReadingMs != 0 &&
      (int32_t)(plantowerNextReadingMs - now) > 0) {
    nextSec = (uint32_t)(plantowerNextReadingMs - now) / 1000;
  }
  data += "\"serial\":\"" + ag->deviceId() + "\",";
  data += "\"hostname\":\"" + getHostname() + ".local\",";
  data += "\"ssid\":\"" + currentSsid() + "\",";
  data += "\"localIp\":\"" + wifiConnector.localIpStr() + "\",";
  data += "\"wifiConnected\":" +
          String(wifiConnector.isConnected() ? "true" : "false") + ",";
  data += "\"wifiRssiDbm\":" + String(rssi) + ",";
  data += "\"wifiSignal\":\"" + wifiSignalLabel(rssi) + "\",";
  data += "\"pm01\":" + String(measure.get(Measurements::PM01)) + ",";
  data += "\"pm25\":" + String(measure.get(Measurements::PM25)) + ",";
  data += "\"pm10\":" + String(measure.get(Measurements::PM10)) + ",";
  data += "\"pm003Count\":" + String(measure.get(Measurements::PM03_PC)) + ",";
  {
    float atmpJson = measure.getFloat(Measurements::Temperature);
    float rhumJson = measure.getFloat(Measurements::Humidity);
    if (utils::isValidTemperature(atmpJson)) {
      data += "\"atmp\":" + String(atmpJson, 1) + ",";
    } else {
      data += "\"atmp\":null,";
    }
    if (utils::isValidHumidity(rhumJson)) {
      data += "\"rhum\":" + String(rhumJson, 0) + ",";
    } else {
      data += "\"rhum\":null,";
    }
  }
  data += "\"tempHumSource\":\"open-meteo\",";
  data += "\"openMeteoLat\":" + String(openMeteoLat, 4) + ",";
  data += "\"openMeteoLon\":" + String(openMeteoLon, 4) + ",";
  data += "\"lastReadingAgeSec\":" + String(ageSec) + ",";
  data += "\"nextReadingSec\":" + String(nextSec) + ",";
  data += "\"uploadSec\":" + String(config.getPlantowerUploadIntervalSec()) +
          ",";
  data += "\"pmReadSec\":" + String(config.getPlantowerPmReadIntervalSec()) +
          ",";
  data += "\"mode\":\"" + config.getPlantowerSensorMode() + "\",";
  data += "\"warmupSec\":" + String(config.getPlantowerWarmupSec());
  data += "}";
  server.send(200, "application/json", data);
}

void LocalServer::_POST_plantower_settings(void) {
  int uploadSec = server.hasArg("uploadSec")
                      ? server.arg("uploadSec").toInt()
                      : config.getPlantowerUploadIntervalSec();
  int pmReadSec = server.hasArg("pmReadSec")
                      ? server.arg("pmReadSec").toInt()
                      : config.getPlantowerPmReadIntervalSec();
  int warmupSec = server.hasArg("warmupSec")
                      ? server.arg("warmupSec").toInt()
                      : config.getPlantowerWarmupSec();
  String mode = server.hasArg("mode") ? server.arg("mode")
                                      : config.getPlantowerSensorMode();
  config.setPlantowerLocalSettings(uploadSec, pmReadSec, mode, warmupSec);
  server.sendHeader("Location", "/network?saved=1");
  server.send(303, "text/plain", "");
}

void LocalServer::_POST_wifi(void) {
  String ssid = server.arg("ssid");
  String password = server.arg("password");
  ssid.trim();
  if (ssid.length() == 0) {
    server.sendHeader("Location", "/network");
    server.send(303, "text/plain", "");
    return;
  }

  WiFi.persistent(true);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, true);
  delay(200);
  WiFi.begin(ssid.c_str(), password.c_str());
  delay(300);
  WiFi.persistent(false);

  String html = F("<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'><title>Saving Wi-Fi</title></head><body style='font-family:sans-serif;padding:24px'><h1>Saving Wi-Fi</h1><p>The board is rebooting onto <b>");
  html += ssid;
  html += F("</b>. Give it about 30 seconds, then open this page on your home network.</p><p>If it does not appear, join the setup hotspot and try again.</p></body></html>");
  rebootAfterReply(html);
}

void LocalServer::_POST_location(void) {
  float lat = server.arg("lat").toFloat();
  float lon = server.arg("lon").toFloat();
  if (lat < -90.0f || lat > 90.0f || lon < -180.0f || lon > 180.0f ||
      (lat == 0.0f && lon == 0.0f)) {
    server.sendHeader("Location", "/network");
    server.send(303, "text/plain", "");
    return;
  }
  openMeteoLat = lat;
  openMeteoLon = lon;
  if (saveOpenMeteoLocation) {
    saveOpenMeteoLocation(lat, lon);
  }
  server.sendHeader("Location", "/network?saved=1");
  server.send(303, "text/plain", "");
}

void LocalServer::_POST_wifi_setup(void) {
  wifiConnector.reset();
  WiFi.persistent(true);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, true);
  delay(200);
  WiFi.persistent(false);

  String html = F("<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'><title>Opening setup hotspot</title></head><body style='font-family:sans-serif;padding:24px'><h1>Opening setup hotspot</h1><p>Join <code>airgradient-");
  html += ag->deviceId();
  html += F("</code> with password <code>cleanair</code>, then open <code>http://192.168.4.1</code>.</p></body></html>");
  rebootAfterReply(html);
}

void LocalServer::setFwMode(AgFirmwareMode fwMode) { this->fwMode = fwMode; }
