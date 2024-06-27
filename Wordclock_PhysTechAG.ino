/*
  Release 1.01 20221005
  Fixed bug preventing wifi details being saved  - moved EEPROM.begin to setup
  Added some more debugging comments

  Release history
  Release 1.0 - fingers crossed

*/
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
//#include <ESP8266WiFi.h>
#include <WiFi.h>
#include <Wire.h>  // Only needed for Arduino 1.6.5 and earlier
#include "FS.h"
#include <Adafruit_GFX.h>
#include <Adafruit_NeoMatrix.h>
#ifndef PSTR
#define PSTR // Make Arduino Due happy
#endif
//#include <ESP8266WiFi.h>
//#include <ESP8266WebServer.h>
#include <WebServer.h>
#include <EEPROM.h>
#include "ezTime.h"
#include "SPIFFS.h"


/*********
  Wifi Config
********/
//ESP8266WebServer    server(80);
WebServer    server(80);
struct settings {
  char ssid[32];
  char password[63];
} user_wifi = {};



/*********
  Timezone Config
********/

char mylocation[40];
Timezone myTZ;



/*********
  Pin Config
********/
/*
#define LED_PIN    D6
#define buttonPin  D5
*/
#define LED_PIN    13
#define buttonPin   0
#define LED_COUNT 256

#define MAX_IDX (LED_COUNT - 1)


/*********
  LED Config
********/
// Declare our NeoPixel strip object:
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRBW + NEO_KHZ800);
//Declare matrix layout
//Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(16, 16, LED_PIN, NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG, NEO_RGBW + NEO_KHZ800);
Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(16, 16, LED_PIN, NEO_MATRIX_TOP + NEO_MATRIX_RIGHT + NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG, NEO_RGBW + NEO_KHZ800);



/*********
  Default Variable Config
********/
//bootmode 0 = user wifi boot, 1 = make access point to configure wifi
int bootmode = 0;
int buttonpressedtype = 0;
int lights[256];
int clockhour = 0;
int clockminute = 0;
int clockseconds = 0;
int displayseconds = 1;
int displayautodim = 0;
int dimstart = 22;
int dimend = 7;
int displayanimation = 1;
long randNumber;

//black
uint32_t pixelcolor = strip.Color(0, 0, 0);

//for running and setting management
int mode = 0;
int changeamount = 1;
int R = 255;
int G = 255;
int B = 255;
int step = 32;
int brightness = 50;
int factoryReset = 0;
uint32_t clockcolor;



// Button timing variables
int debounce = 20; // ms debounce period to prevent flickering when pressing or releasing the button
int DCgap = 250; // max ms between clicks for a double click event
int holdTime = 2000; // ms hold period: how long to wait for press+hold event
int longHoldTime = 5000; // ms long hold period: how long to wait for press+hold event

// Other button variables
boolean buttonVal = HIGH; // value read from button
boolean buttonLast = HIGH; // buffered value of the button's previous state
boolean DCwaiting = false; // whether we're waiting for a double click (down)
boolean DConUp = false; // whether to register a double click on next release, or whether to wait and click
boolean singleOK = true; // whether it's OK to do a single click
long downTime = -1; // time the button was pressed down
long upTime = -1; // time the button was released
boolean ignoreUp = false; // whether to ignore the button release because the click+hold was triggered
boolean waitForUp = false; // when held, whether to wait for the up event
boolean holdEventPast = false; // whether or not the hold event happened already
boolean longHoldEventPast = false;// whether or not the long hold event happened already








//#define countof(a) (sizeof(a) / sizeof(a[0]))



bool loadConfig() {
  File configFile = SPIFFS.open("/config.json", "r");
  if (!configFile) {
    Serial.println("Failed to open config file");
    return false;
  }

  size_t size = configFile.size();
  if (size > 1024) {
    Serial.println("Config file size is too large");
    return false;
  }

  // Allocate a buffer to store contents of the file.
  std::unique_ptr<char[]> buf(new char[size]);

  // We don't use String here because ArduinoJson library requires the input
  // buffer to be mutable. If you don't use ArduinoJson, you may as well
  // use configFile.readString instead.
  configFile.readBytes(buf.get(), size);

  StaticJsonDocument<200> doc;
  auto error = deserializeJson(doc, buf.get());
  if (error) {
    Serial.println("Failed to parse config file");
    return false;
  }

  displayseconds = doc["dispS"];
  R = doc["R"];
  G = doc["G"];
  B = doc["B"];
  brightness =  doc["Br"];
  displayautodim =  doc["dispAD"];
  dimstart = doc["dimstart"];
  dimend = doc["dimend"];
  displayanimation = doc["dispAn"];
  strncpy(mylocation, doc["mylocation"], sizeof(mylocation));
  bootmode = doc["bootmode"];

  Serial.println("Loaded bootmode is");
  Serial.println(bootmode);


  clockcolor = strip.Color(R, G, B);
  strip.setBrightness(brightness);

  Serial.println("Loaded config ");

  return true;
}

bool saveConfig() {
  StaticJsonDocument<200> doc;
  Serial.println("Got to saveConfig");
  if (factoryReset != 1) {

    Serial.println("Updated user config variables");
    doc["dispS"] = displayseconds;
    doc["R"] = R;
    doc["G"] = G;
    doc["B"] = B;
    doc["Br"] = brightness;
    doc["dispAD"] = displayautodim;
    doc["dimstart"] = dimstart;
    doc["dimend"] = dimend;
    doc["dispAn"] = displayanimation;
    doc["mylocation"] = mylocation;
    doc["bootmode"] = 0;


  } else {
    //set factory reset values
    doc["dispS"] = 1;
    doc["R"] = 255;
    doc["G"] = 255;
    doc["B"] = 255;
    doc["Br"] = 50;
    doc["dispAD"] = 0;
    doc["dimstart"] = 22;
    doc["dimend"] = 7;
    doc["dispAn"] = 1;
    doc["mylocation"] = "Europe/London";
    doc["bootmode"] = 1;

    factoryReset = 0;
  }

  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    return false;
  }

  serializeJson(doc, configFile);
  return true;
}

// convert a hex character to number
uint8_t char2num(char c)
{
  if (c >= 'a') return c - 'a' + 10;
  if (c >= 'A') return c - 'A' + 10;
  return c - '0';
}

void handlePortal() {

  if (server.method() == HTTP_POST) {
    strncpy(user_wifi.ssid, server.arg("ssid").c_str(), sizeof(user_wifi.ssid));
    strncpy(user_wifi.password, server.arg("password").c_str(), sizeof(user_wifi.password));
    user_wifi.ssid[server.arg("ssid").length()] = user_wifi.password[server.arg("password").length()] = '\0';
    EEPROM.put(0, user_wifi);
    if (EEPROM.commit()) {
      Serial.println("EEPROM successfully committed");
    } else {
      Serial.println("ERROR! EEPROM commit failed");
    }


    strncpy(mylocation, server.arg("location").c_str(), sizeof(mylocation) );

    char clockcolorraw[7];
    char clockcolor[6];
    strncpy(clockcolorraw, server.arg("clockcolor").c_str(), sizeof(clockcolorraw));
    //for (int i = 1 ; i < 7 ; i++) clockcolor[(i - 1)] = clockcolorraw[i];
    R = 16 * char2num(clockcolorraw[1]) + char2num(clockcolorraw[2]);
    G = 16 * char2num(clockcolorraw[3]) + char2num(clockcolorraw[4]);
    B = 16 * char2num(clockcolorraw[5]) + char2num(clockcolorraw[6]);
    char temp[3];
    strncpy(temp, server.arg("displayseconds").c_str(), sizeof(temp));
    displayseconds = temp[0] - 48;
    strncpy(temp, server.arg("brightness").c_str(), sizeof(temp));
    brightness = atoi(temp);
    strncpy(temp, server.arg("displayautodim").c_str(), sizeof(temp));
    displayautodim = temp[0] - 48;
    strncpy(temp, server.arg("dimstart").c_str(), sizeof(temp));
    dimstart = atoi(temp);
    strncpy(temp, server.arg("dimend").c_str(), sizeof(temp));
    dimend = atoi(temp);
    strncpy(temp, server.arg("displayanimation").c_str(), sizeof(temp));
    displayanimation = temp[0] - 48;

    Serial.println(dimstart);
    Serial.println(dimend);



    server.send(200,   "text/html",  "<!doctype html><html lang='en'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'><title>Clock Setup</title><style>*,::after,::before{box-sizing:border-box;}body{margin:0;font-family:'Segoe UI',Roboto,'Helvetica Neue',Arial,'Noto Sans','Liberation Sans';font-size:1rem;font-weight:400;line-height:1.5;color:#212529;background-color:#f5f5f5;}.form-control{display:block;width:100%;height:calc(1.5em + .75rem + 2px);border:1px solid #ced4da;}button{border:1px solid transparent;color:#fff;background-color:#007bff;border-color:#007bff;padding:.5rem 1rem;font-size:1.25rem;line-height:1.5;border-radius:.3rem;width:100%}.form-signin{width:100%;max-width:400px;padding:15px;margin:auto;}h1,p{text-align: center}</style> </head> <body><main class='form-signin'> <h1>Wifi Setup</h1> <br/> <p>Your settings have been saved successfully!<br />Please restart the device if it does not restart itself within 30 seconds.</p></main></body></html>" );

    factoryReset = 0;
    //sets back config to basic config and enables wifi access point.
    saveConfig();
    delay(5000);
    ESP.restart();

  } else {

    // server.send(200, "text/html", );
    server.send(200, "text/html", F("<!doctype html><html lang='en'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'><title>Clock Setup</title><style>*,::after,::before{box-sizing:border-box;}body{margin:0;font-family:'Segoe UI',Roboto,'Helvetica Neue',Arial,'Noto Sans','Liberation Sans';font-size:1rem;font-weight:400;line-height:1.5;color:#212529;background-color:#f5f5f5;}.form-control{display:block;width:100%;height:calc(1.5em + .75rem + 2px);border:1px solid #ced4da;}button{cursor: pointer;border:1px solid transparent;color:#fff;background-color:#007bff;border-color:#007bff;padding:.5rem 1rem;font-size:1.25rem;line-height:1.5;border-radius:.3rem;width:100%}.form-inline{input[type=radio] {display: inline;};;input, textarea,select {margin-top: 10px; display: block;}}.form-signin{width:100%;max-width:400px;padding:15px;margin:auto;}h1{text-align: center}</style></head><body><main class='form-signin'><form action='/' method='post'><h1 class=''>Clock Setup</h1><br/><div class='form-floating'><label>Enter your Wifi SSID</label><input type='text' class='form-control' name='ssid'></div><div class='form-floating'><br/><label>Enter your Wifi Password</label><input type='password' class='form-control' name='password'><br/><h4>Note this is not being sent by https or ssl</h4></div><br/><div class='form-inline'><label for='location'>Select your timezone</label><select name='location' id='location'><option value='Africa/Accra'>Africa/Accra</option><option value='Africa/Addis_Ababa'>Africa/Addis_Ababa</option><option value='Africa/Algiers'>Africa/Algiers</option><option value='Africa/Asmara'>Africa/Asmara</option><option value='Africa/Asmera'>Africa/Asmera</option><option value='Africa/Bamako'>Africa/Bamako</option><option value='Africa/Bangui'>Africa/Bangui</option><option value='Africa/Banjul'>Africa/Banjul</option><option value='Africa/Bissau'>Africa/Bissau</option><option value='Africa/Blantyre'>Africa/Blantyre</option><option value='Africa/Brazzaville'>Africa/Brazzaville</option><option value='Africa/Bujumbura'>Africa/Bujumbura</option><option value='Africa/Cairo'>Africa/Cairo</option><option value='Africa/Casablanca'>Africa/Casablanca</option><option value='Africa/Ceuta'>Africa/Ceuta</option><option value='Africa/Conakry'>Africa/Conakry</option><option value='Africa/Dakar'>Africa/Dakar</option><option value='Africa/Dar_es_Salaam'>Africa/Dar_es_Salaam</option><option value='Africa/Djibouti'>Africa/Djibouti</option><option value='Africa/Douala'>Africa/Douala</option><option value='Africa/El_Aaiun'>Africa/El_Aaiun</option><option value='Africa/Freetown'>Africa/Freetown</option><option value='Africa/Gaborone'>Africa/Gaborone</option><option value='Africa/Harare'>Africa/Harare</option><option value='Africa/Johannesburg'>Africa/Johannesburg</option><option value='Africa/Juba'>Africa/Juba</option><option value='Africa/Kampala'>Africa/Kampala</option><option value='Africa/Khartoum'>Africa/Khartoum</option><option value='Africa/Kigali'>Africa/Kigali</option><option value='Africa/Kinshasa'>Africa/Kinshasa</option><option value='Africa/Lagos'>Africa/Lagos</option><option value='Africa/Libreville'>Africa/Libreville</option><option value='Africa/Lome'>Africa/Lome</option><option value='Africa/Luanda'>Africa/Luanda</option><option value='Africa/Lubumbashi'>Africa/Lubumbashi</option><option value='Africa/Lusaka'>Africa/Lusaka</option><option value='Africa/Malabo'>Africa/Malabo</option><option value='Africa/Maputo'>Africa/Maputo</option><option value='Africa/Maseru'>Africa/Maseru</option><option value='Africa/Mbabane'>Africa/Mbabane</option><option value='Africa/Mogadishu'>Africa/Mogadishu</option><option value='Africa/Monrovia'>Africa/Monrovia</option><option value='Africa/Nairobi'>Africa/Nairobi</option><option value='Africa/Ndjamena'>Africa/Ndjamena</option><option value='Africa/Niamey'>Africa/Niamey</option><option value='Africa/Nouakchott'>Africa/Nouakchott</option><option value='Africa/Ouagadougou'>Africa/Ouagadougou</option><option value='Africa/Porto-Novo'>Africa/Porto-Novo</option><option value='Africa/Sao_Tome'>Africa/Sao_Tome</option><option value='Africa/Timbuktu'>Africa/Timbuktu</option><option value='Africa/Tripoli'>Africa/Tripoli</option><option value='Africa/Tunis'>Africa/Tunis</option><option value='Africa/Windhoek'>Africa/Windhoek</option><option value='America/Adak'>America/Adak</option><option value='America/Anchorage'>America/Anchorage</option><option value='America/Anguilla'>America/Anguilla</option><option value='America/Antigua'>America/Antigua</option><option value='America/Araguaina'>America/Araguaina</option><option value='America/Argentina/Buenos_Aires'>America/Argentina/Buenos_Aires</option><option value='America/Argentina/Catamarca'>America/Argentina/Catamarca</option><option value='America/Argentina/ComodRivadavia'>America/Argentina/ComodRivadavia</option><option value='America/Argentina/Cordoba'>America/Argentina/Cordoba</option><option value='America/Argentina/Jujuy'>America/Argentina/Jujuy</option><option value='America/Argentina/La_Rioja'>America/Argentina/La_Rioja</option><option value='America/Argentina/Mendoza'>America/Argentina/Mendoza</option><option value='America/Argentina/Rio_Gallegos'>America/Argentina/Rio_Gallegos</option><option value='America/Argentina/Salta'>America/Argentina/Salta</option><option value='America/Argentina/San_Juan'>America/Argentina/San_Juan</option><option value='America/Argentina/San_Luis'>America/Argentina/San_Luis</option><option value='America/Argentina/Tucuman'>America/Argentina/Tucuman</option><option value='America/Argentina/Ushuaia'>America/Argentina/Ushuaia</option><option value='America/Aruba'>America/Aruba</option><option value='America/Asuncion'>America/Asuncion</option><option value='America/Atikokan'>America/Atikokan</option><option value='America/Atka'>America/Atka</option><option value='America/Bahia'>America/Bahia</option><option value='America/Bahia_Banderas'>America/Bahia_Banderas</option><option value='America/Barbados'>America/Barbados</option><option value='America/Belem'>America/Belem</option><option value='America/Belize'>America/Belize</option><option value='America/Blanc-Sablon'>America/Blanc-Sablon</option><option value='America/Boa_Vista'>America/Boa_Vista</option><option value='America/Bogota'>America/Bogota</option><option value='America/Boise'>America/Boise</option><option value='America/Buenos_Aires'>America/Buenos_Aires</option><option value='America/Cambridge_Bay'>America/Cambridge_Bay</option><option value='America/Campo_Grande'>America/Campo_Grande</option><option value='America/Cancun'>America/Cancun</option><option value='America/Caracas'>America/Caracas</option><option value='America/Catamarca'>America/Catamarca</option><option value='America/Cayenne'>America/Cayenne</option><option value='America/Cayman'>America/Cayman</option><option value='America/Chicago'>America/Chicago</option><option value='America/Chihuahua'>America/Chihuahua</option><option value='America/Coral_Harbour'>America/Coral_Harbour</option><option value='America/Cordoba'>America/Cordoba</option><option value='America/Costa_Rica'>America/Costa_Rica</option><option value='America/Creston'>America/Creston</option><option value='America/Cuiaba'>America/Cuiaba</option><option value='America/Curacao'>America/Curacao</option><option value='America/Danmarkshavn'>America/Danmarkshavn</option><option value='America/Dawson'>America/Dawson</option><option value='America/Dawson_Creek'>America/Dawson_Creek</option><option value='America/Denver'>America/Denver</option><option value='America/Detroit'>America/Detroit</option><option value='America/Dominica'>America/Dominica</option><option value='America/Edmonton'>America/Edmonton</option><option value='America/Eirunepe'>America/Eirunepe</option><option value='America/El_Salvador'>America/El_Salvador</option><option value='America/Ensenada'>America/Ensenada</option><option value='America/Fort_Nelson'>America/Fort_Nelson</option><option value='America/Fort_Wayne'>America/Fort_Wayne</option><option value='America/Fortaleza'>America/Fortaleza</option><option value='America/Glace_Bay'>America/Glace_Bay</option><option value='America/Godthab'>America/Godthab</option><option value='America/Goose_Bay'>America/Goose_Bay</option><option value='America/Grand_Turk'>America/Grand_Turk</option><option value='America/Grenada'>America/Grenada</option><option value='America/Guadeloupe'>America/Guadeloupe</option><option value='America/Guatemala'>America/Guatemala</option><option value='America/Guayaquil'>America/Guayaquil</option><option value='America/Guyana'>America/Guyana</option><option value='America/Halifax'>America/Halifax</option><option value='America/Havana'>America/Havana</option><option value='America/Hermosillo'>America/Hermosillo</option><option value='America/Indiana/Indianapolis'>America/Indiana/Indianapolis</option><option value='America/Indiana/Knox'>America/Indiana/Knox</option><option value='America/Indiana/Marengo'>America/Indiana/Marengo</option><option value='America/Indiana/Petersburg'>America/Indiana/Petersburg</option><option value='America/Indiana/Tell_City'>America/Indiana/Tell_City</option><option value='America/Indiana/Vevay'>America/Indiana/Vevay</option><option value='America/Indiana/Vincennes'>America/Indiana/Vincennes</option><option value='America/Indiana/Winamac'>America/Indiana/Winamac</option><option value='America/Indianapolis'>America/Indianapolis</option><option value='America/Inuvik'>America/Inuvik</option><option value='America/Iqaluit'>America/Iqaluit</option><option value='America/Jamaica'>America/Jamaica</option><option value='America/Jujuy'>America/Jujuy</option><option value='America/Juneau'>America/Juneau</option><option value='America/Kentucky/Louisville'>America/Kentucky/Louisville</option><option value='America/Kentucky/Monticello'>America/Kentucky/Monticello</option><option value='America/Knox_IN'>America/Knox_IN</option><option value='America/Kralendijk'>America/Kralendijk</option><option value='America/La_Paz'>America/La_Paz</option><option value='America/Lima'>America/Lima</option><option value='America/Los_Angeles'>America/Los_Angeles</option><option value='America/Louisville'>America/Louisville</option><option value='America/Lower_Princes'>America/Lower_Princes</option><option value='America/Maceio'>America/Maceio</option><option value='America/Managua'>America/Managua</option><option value='America/Manaus'>America/Manaus</option><option value='America/Marigot'>America/Marigot</option><option value='America/Martinique'>America/Martinique</option><option value='America/Matamoros'>America/Matamoros</option><option value='America/Mazatlan'>America/Mazatlan</option><option value='America/Mendoza'>America/Mendoza</option><option value='America/Menominee'>America/Menominee</option><option value='America/Merida'>America/Merida</option><option value='America/Metlakatla'>America/Metlakatla</option><option value='America/Mexico_City'>America/Mexico_City</option><option value='America/Miquelon'>America/Miquelon</option><option value='America/Moncton'>America/Moncton</option><option value='America/Monterrey'>America/Monterrey</option><option value='America/Montevideo'>America/Montevideo</option><option value='America/Montreal'>America/Montreal</option><option value='America/Montserrat'>America/Montserrat</option><option value='America/Nassau'>America/Nassau</option><option value='America/New_York'>America/New_York</option><option value='America/Nipigon'>America/Nipigon</option><option value='America/Nome'>America/Nome</option><option value='America/Noronha'>America/Noronha</option><option value='America/North_Dakota/Beulah'>America/North_Dakota/Beulah</option><option value='America/North_Dakota/Center'>America/North_Dakota/Center</option><option value='America/North_Dakota/New_Salem'>America/North_Dakota/New_Salem</option><option value='America/Nuuk'>America/Nuuk</option><option value='America/Ojinaga'>America/Ojinaga</option><option value='America/Panama'>America/Panama</option><option value='America/Pangnirtung'>America/Pangnirtung</option><option value='America/Paramaribo'>America/Paramaribo</option><option value='America/Phoenix'>America/Phoenix</option><option value='America/Port-au-Prince'>America/Port-au-Prince</option><option value='America/Port_of_Spain'>America/Port_of_Spain</option><option value='America/Porto_Acre'>America/Porto_Acre</option><option value='America/Porto_Velho'>America/Porto_Velho</option><option value='America/Puerto_Rico'>America/Puerto_Rico</option><option value='America/Punta_Arenas'>America/Punta_Arenas</option><option value='America/Rainy_River'>America/Rainy_River</option><option value='America/Rankin_Inlet'>America/Rankin_Inlet</option><option value='America/Recife'>America/Recife</option><option value='America/Regina'>America/Regina</option><option value='America/Resolute'>America/Resolute</option><option value='America/Rio_Branco'>America/Rio_Branco</option><option value='America/Rosario'>America/Rosario</option><option value='America/Santa_Isabel'>America/Santa_Isabel</option><option value='America/Santarem'>America/Santarem</option><option value='America/Santiago'>America/Santiago</option><option value='America/Santo_Domingo'>America/Santo_Domingo</option><option value='America/Sao_Paulo'>America/Sao_Paulo</option><option value='America/Scoresbysund'>America/Scoresbysund</option><option value='America/Shiprock'>America/Shiprock</option><option value='America/Sitka'>America/Sitka</option><option value='America/St_Barthelemy'>America/St_Barthelemy</option><option value='America/St_Johns'>America/St_Johns</option><option value='America/St_Kitts'>America/St_Kitts</option><option value='America/St_Lucia'>America/St_Lucia</option><option value='America/St_Thomas'>America/St_Thomas</option><option value='America/St_Vincent'>America/St_Vincent</option><option value='America/Swift_Current'>America/Swift_Current</option><option value='America/Tegucigalpa'>America/Tegucigalpa</option><option value='America/Thule'>America/Thule</option><option value='America/Thunder_Bay'>America/Thunder_Bay</option><option value='America/Tijuana'>America/Tijuana</option><option value='America/Toronto'>America/Toronto</option><option value='America/Tortola'>America/Tortola</option><option value='America/Vancouver'>America/Vancouver</option><option value='America/Virgin'>America/Virgin</option><option value='America/Whitehorse'>America/Whitehorse</option><option value='America/Winnipeg'>America/Winnipeg</option><option value='America/Yakutat'>America/Yakutat</option><option value='America/Yellowknife'>America/Yellowknife</option><option value='Antarctica/Casey'>Antarctica/Casey</option><option value='Antarctica/Davis'>Antarctica/Davis</option><option value='Antarctica/DumontDUrville'>Antarctica/DumontDUrville</option><option value='Antarctica/Macquarie'>Antarctica/Macquarie</option><option value='Antarctica/Mawson'>Antarctica/Mawson</option><option value='Antarctica/McMurdo'>Antarctica/McMurdo</option><option value='Antarctica/Palmer'>Antarctica/Palmer</option><option value='Antarctica/Rothera'>Antarctica/Rothera</option><option value='Antarctica/South_Pole'>Antarctica/South_Pole</option><option value='Antarctica/Syowa'>Antarctica/Syowa</option><option value='Antarctica/Troll'>Antarctica/Troll</option><option value='Antarctica/Vostok'>Antarctica/Vostok</option><option value='Arctic/Longyearbyen'>Arctic/Longyearbyen</option><option value='Asia/Aden'>Asia/Aden</option><option value='Asia/Almaty'>Asia/Almaty</option><option value='Asia/Amman'>Asia/Amman</option><option value='Asia/Anadyr'>Asia/Anadyr</option><option value='Asia/Aqtau'>Asia/Aqtau</option><option value='Asia/Aqtobe'>Asia/Aqtobe</option><option value='Asia/Ashgabat'>Asia/Ashgabat</option><option value='Asia/Ashkhabad'>Asia/Ashkhabad</option><option value='Asia/Atyrau'>Asia/Atyrau</option><option value='Asia/Baghdad'>Asia/Baghdad</option><option value='Asia/Bahrain'>Asia/Bahrain</option><option value='Asia/Baku'>Asia/Baku</option><option value='Asia/Bangkok'>Asia/Bangkok</option><option value='Asia/Barnaul'>Asia/Barnaul</option><option value='Asia/Beirut'>Asia/Beirut</option><option value='Asia/Bishkek'>Asia/Bishkek</option><option value='Asia/Brunei'>Asia/Brunei</option><option value='Asia/Calcutta'>Asia/Calcutta</option><option value='Asia/Chita'>Asia/Chita</option><option value='Asia/Choibalsan'>Asia/Choibalsan</option><option value='Asia/Chongqing'>Asia/Chongqing</option><option value='Asia/Chungking'>Asia/Chungking</option><option value='Asia/Colombo'>Asia/Colombo</option><option value='Asia/Dacca'>Asia/Dacca</option><option value='Asia/Damascus'>Asia/Damascus</option><option value='Asia/Dhaka'>Asia/Dhaka</option><option value='Asia/Dili'>Asia/Dili</option><option value='Asia/Dubai'>Asia/Dubai</option><option value='Asia/Dushanbe'>Asia/Dushanbe</option><option value='Asia/Famagusta'>Asia/Famagusta</option><option value='Asia/Gaza'>Asia/Gaza</option><option value='Asia/Harbin'>Asia/Harbin</option><option value='Asia/Hebron'>Asia/Hebron</option><option value='Asia/Ho_Chi_Minh'>Asia/Ho_Chi_Minh</option><option value='Asia/Hong_Kong'>Asia/Hong_Kong</option><option value='Asia/Hovd'>Asia/Hovd</option><option value='Asia/Irkutsk'>Asia/Irkutsk</option><option value='Asia/Istanbul'>Asia/Istanbul</option><option value='Asia/Jakarta'>Asia/Jakarta</option><option value='Asia/Jayapura'>Asia/Jayapura</option><option value='Asia/Jerusalem'>Asia/Jerusalem</option><option value='Asia/Kabul'>Asia/Kabul</option><option value='Asia/Kamchatka'>Asia/Kamchatka</option><option value='Asia/Karachi'>Asia/Karachi</option><option value='Asia/Kashgar'>Asia/Kashgar</option><option value='Asia/Kathmandu'>Asia/Kathmandu</option><option value='Asia/Katmandu'>Asia/Katmandu</option><option value='Asia/Khandyga'>Asia/Khandyga</option><option value='Asia/Kolkata'>Asia/Kolkata</option><option value='Asia/Krasnoyarsk'>Asia/Krasnoyarsk</option><option value='Asia/Kuala_Lumpur'>Asia/Kuala_Lumpur</option><option value='Asia/Kuching'>Asia/Kuching</option><option value='Asia/Kuwait'>Asia/Kuwait</option><option value='Asia/Macao'>Asia/Macao</option><option value='Asia/Macau'>Asia/Macau</option><option value='Asia/Magadan'>Asia/Magadan</option><option value='Asia/Makassar'>Asia/Makassar</option><option value='Asia/Manila'>Asia/Manila</option><option value='Asia/Muscat'>Asia/Muscat</option><option value='Asia/Nicosia'>Asia/Nicosia</option><option value='Asia/Novokuznetsk'>Asia/Novokuznetsk</option><option value='Asia/Novosibirsk'>Asia/Novosibirsk</option><option value='Asia/Omsk'>Asia/Omsk</option><option value='Asia/Oral'>Asia/Oral</option><option value='Asia/Phnom_Penh'>Asia/Phnom_Penh</option><option value='Asia/Pontianak'>Asia/Pontianak</option><option value='Asia/Pyongyang'>Asia/Pyongyang</option><option value='Asia/Qatar'>Asia/Qatar</option><option value='Asia/Qostanay'>Asia/Qostanay</option><option value='Asia/Qyzylorda'>Asia/Qyzylorda</option><option value='Asia/Rangoon'>Asia/Rangoon</option><option value='Asia/Riyadh'>Asia/Riyadh</option><option value='Asia/Saigon'>Asia/Saigon</option><option value='Asia/Sakhalin'>Asia/Sakhalin</option><option value='Asia/Samarkand'>Asia/Samarkand</option><option value='Asia/Seoul'>Asia/Seoul</option><option value='Asia/Shanghai'>Asia/Shanghai</option><option value='Asia/Singapore'>Asia/Singapore</option><option value='Asia/Srednekolymsk'>Asia/Srednekolymsk</option><option value='Asia/Taipei'>Asia/Taipei</option><option value='Asia/Tashkent'>Asia/Tashkent</option><option value='Asia/Tbilisi'>Asia/Tbilisi</option><option value='Asia/Tehran'>Asia/Tehran</option><option value='Asia/Tel_Aviv'>Asia/Tel_Aviv</option><option value='Asia/Thimbu'>Asia/Thimbu</option><option value='Asia/Thimphu'>Asia/Thimphu</option><option value='Asia/Tokyo'>Asia/Tokyo</option><option value='Asia/Tomsk'>Asia/Tomsk</option><option value='Asia/Ujung_Pandang'>Asia/Ujung_Pandang</option><option value='Asia/Ulaanbaatar'>Asia/Ulaanbaatar</option><option value='Asia/Ulan_Bator'>Asia/Ulan_Bator</option><option value='Asia/Urumqi'>Asia/Urumqi</option><option value='Asia/Ust-Nera'>Asia/Ust-Nera</option><option value='Asia/Vientiane'>Asia/Vientiane</option><option value='Asia/Vladivostok'>Asia/Vladivostok</option><option value='Asia/Yakutsk'>Asia/Yakutsk</option><option value='Asia/Yangon'>Asia/Yangon</option><option value='Asia/Yekaterinburg'>Asia/Yekaterinburg</option><option value='Asia/Yerevan'>Asia/Yerevan</option><option value='Atlantic/Azores'>Atlantic/Azores</option><option value='Atlantic/Bermuda'>Atlantic/Bermuda</option><option value='Atlantic/Canary'>Atlantic/Canary</option><option value='Atlantic/Cape_Verde'>Atlantic/Cape_Verde</option><option value='Atlantic/Faeroe'>Atlantic/Faeroe</option><option value='Atlantic/Faroe'>Atlantic/Faroe</option><option value='Atlantic/Jan_Mayen'>Atlantic/Jan_Mayen</option><option value='Atlantic/Madeira'>Atlantic/Madeira</option><option value='Atlantic/Reykjavik'>Atlantic/Reykjavik</option><option value='Atlantic/South_Georgia'>Atlantic/South_Georgia</option><option value='Atlantic/St_Helena'>Atlantic/St_Helena</option><option value='Atlantic/Stanley'>Atlantic/Stanley</option><option value='Australia/ACT'>Australia/ACT</option><option value='Australia/Adelaide'>Australia/Adelaide</option><option value='Australia/Brisbane'>Australia/Brisbane</option><option value='Australia/Broken_Hill'>Australia/Broken_Hill</option><option value='Australia/Canberra'>Australia/Canberra</option><option value='Australia/Currie'>Australia/Currie</option><option value='Australia/Darwin'>Australia/Darwin</option><option value='Australia/Eucla'>Australia/Eucla</option><option value='Australia/Hobart'>Australia/Hobart</option><option value='Australia/LHI'>Australia/LHI</option><option value='Australia/Lindeman'>Australia/Lindeman</option><option value='Australia/Lord_Howe'>Australia/Lord_Howe</option><option value='Australia/Melbourne'>Australia/Melbourne</option><option value='Australia/North'>Australia/North</option><option value='Australia/NSW'>Australia/NSW</option><option value='Australia/Perth'>Australia/Perth</option><option value='Australia/Queensland'>Australia/Queensland</option><option value='Australia/South'>Australia/South</option><option value='Australia/Sydney'>Australia/Sydney</option><option value='Australia/Tasmania'>Australia/Tasmania</option><option value='Australia/Victoria'>Australia/Victoria</option><option value='Australia/West'>Australia/West</option><option value='Australia/Yancowinna'>Australia/Yancowinna</option><option value='Brazil/Acre'>Brazil/Acre</option><option value='Brazil/DeNoronha'>Brazil/DeNoronha</option><option value='Brazil/East'>Brazil/East</option><option value='Brazil/West'>Brazil/West</option><option value='Canada/Atlantic'>Canada/Atlantic</option><option value='Canada/Central'>Canada/Central</option><option value='Canada/Eastern'>Canada/Eastern</option><option value='Canada/Mountain'>Canada/Mountain</option><option value='Canada/Newfoundland'>Canada/Newfoundland</option><option value='Canada/Pacific'>Canada/Pacific</option><option value='Canada/Saskatchewan'>Canada/Saskatchewan</option><option value='Canada/Yukon'>Canada/Yukon</option><option value='CET'>CET</option><option value='Chile/Continental'>Chile/Continental</option><option value='Chile/EasterIsland'>Chile/EasterIsland</option><option value='CST6CDT'>CST6CDT</option><option value='Cuba'>Cuba</option><option value='EET'>EET</option><option value='Egypt'>Egypt</option><option value='Eire'>Eire</option><option value='EST'>EST</option><option value='EST5EDT'>EST5EDT</option><option value='Etc/GMT'>Etc/GMT</option><option value='Etc/GMT+0'>Etc/GMT+0</option><option value='Etc/GMT+1'>Etc/GMT+1</option><option value='Etc/GMT+10'>Etc/GMT+10</option><option value='Etc/GMT+11'>Etc/GMT+11</option><option value='Etc/GMT+12'>Etc/GMT+12</option><option value='Etc/GMT+2'>Etc/GMT+2</option><option value='Etc/GMT+3'>Etc/GMT+3</option><option value='Etc/GMT+4'>Etc/GMT+4</option><option value='Etc/GMT+5'>Etc/GMT+5</option><option value='Etc/GMT+6'>Etc/GMT+6</option><option value='Etc/GMT+7'>Etc/GMT+7</option><option value='Etc/GMT+8'>Etc/GMT+8</option><option value='Etc/GMT+9'>Etc/GMT+9</option><option value='Etc/GMT-0'>Etc/GMT-0</option><option value='Etc/GMT-1'>Etc/GMT-1</option><option value='Etc/GMT-10'>Etc/GMT-10</option><option value='Etc/GMT-11'>Etc/GMT-11</option><option value='Etc/GMT-12'>Etc/GMT-12</option><option value='Etc/GMT-13'>Etc/GMT-13</option><option value='Etc/GMT-14'>Etc/GMT-14</option><option value='Etc/GMT-2'>Etc/GMT-2</option><option value='Etc/GMT-3'>Etc/GMT-3</option><option value='Etc/GMT-4'>Etc/GMT-4</option><option value='Etc/GMT-5'>Etc/GMT-5</option><option value='Etc/GMT-6'>Etc/GMT-6</option><option value='Etc/GMT-7'>Etc/GMT-7</option><option value='Etc/GMT-8'>Etc/GMT-8</option><option value='Etc/GMT-9'>Etc/GMT-9</option><option value='Etc/GMT0'>Etc/GMT0</option><option value='Etc/Greenwich'>Etc/Greenwich</option><option value='Etc/UCT'>Etc/UCT</option><option value='Etc/Universal'>Etc/Universal</option><option value='Etc/UTC'>Etc/UTC</option><option value='Etc/Zulu'>Etc/Zulu</option><option value='Europe/Amsterdam'>Europe/Amsterdam</option><option value='Europe/Andorra'>Europe/Andorra</option><option value='Europe/Astrakhan'>Europe/Astrakhan</option><option value='Europe/Athens'>Europe/Athens</option><option value='Europe/Belfast'>Europe/Belfast</option><option value='Europe/Belgrade'>Europe/Belgrade</option><option value='Europe/Berlin'>Europe/Berlin</option><option value='Europe/Bratislava'>Europe/Bratislava</option><option value='Europe/Brussels'>Europe/Brussels</option><option value='Europe/Bucharest'>Europe/Bucharest</option><option value='Europe/Budapest'>Europe/Budapest</option><option value='Europe/Busingen'>Europe/Busingen</option><option value='Europe/Chisinau'>Europe/Chisinau</option><option value='Europe/Copenhagen'>Europe/Copenhagen</option><option value='Europe/Dublin'>Europe/Dublin</option><option value='Europe/Gibraltar'>Europe/Gibraltar</option><option value='Europe/Guernsey'>Europe/Guernsey</option><option value='Europe/Helsinki'>Europe/Helsinki</option><option value='Europe/Isle_of_Man'>Europe/Isle_of_Man</option><option value='Europe/Istanbul'>Europe/Istanbul</option><option value='Europe/Jersey'>Europe/Jersey</option><option value='Europe/Kaliningrad'>Europe/Kaliningrad</option><option value='Europe/Kiev'>Europe/Kiev</option><option value='Europe/Kirov'>Europe/Kirov</option><option value='Europe/Lisbon'>Europe/Lisbon</option><option value='Europe/Ljubljana'>Europe/Ljubljana</option><option value='Europe/London'>Europe/London</option><option value='Europe/Luxembourg'>Europe/Luxembourg</option><option value='Europe/Madrid'>Europe/Madrid</option><option value='Europe/Malta'>Europe/Malta</option><option value='Europe/Mariehamn'>Europe/Mariehamn</option><option value='Europe/Minsk'>Europe/Minsk</option><option value='Europe/Monaco'>Europe/Monaco</option><option value='Europe/Moscow'>Europe/Moscow</option><option value='Europe/Nicosia'>Europe/Nicosia</option><option value='Europe/Oslo'>Europe/Oslo</option><option value='Europe/Paris'>Europe/Paris</option><option value='Europe/Podgorica'>Europe/Podgorica</option><option value='Europe/Prague'>Europe/Prague</option><option value='Europe/Riga'>Europe/Riga</option><option value='Europe/Rome'>Europe/Rome</option><option value='Europe/Samara'>Europe/Samara</option><option value='Europe/San_Marino'>Europe/San_Marino</option><option value='Europe/Sarajevo'>Europe/Sarajevo</option><option value='Europe/Saratov'>Europe/Saratov</option><option value='Europe/Simferopol'>Europe/Simferopol</option><option value='Europe/Skopje'>Europe/Skopje</option><option value='Europe/Sofia'>Europe/Sofia</option><option value='Europe/Stockholm'>Europe/Stockholm</option><option value='Europe/Tallinn'>Europe/Tallinn</option><option value='Europe/Tirane'>Europe/Tirane</option><option value='Europe/Tiraspol'>Europe/Tiraspol</option><option value='Europe/Ulyanovsk'>Europe/Ulyanovsk</option><option value='Europe/Uzhgorod'>Europe/Uzhgorod</option><option value='Europe/Vaduz'>Europe/Vaduz</option><option value='Europe/Vatican'>Europe/Vatican</option><option value='Europe/Vienna'>Europe/Vienna</option><option value='Europe/Vilnius'>Europe/Vilnius</option><option value='Europe/Volgograd'>Europe/Volgograd</option><option value='Europe/Warsaw'>Europe/Warsaw</option><option value='Europe/Zagreb'>Europe/Zagreb</option><option value='Europe/Zaporozhye'>Europe/Zaporozhye</option><option value='Europe/Zurich'>Europe/Zurich</option><option value='Factory'>Factory</option><option value='GB'>GB</option><option value='GB-Eire'>GB-Eire</option><option value='GMT'>GMT</option><option value='GMT+0'>GMT+0</option><option value='GMT-0'>GMT-0</option><option value='GMT0'>GMT0</option><option value='Greenwich'>Greenwich</option><option value='Hongkong'>Hongkong</option><option value='HST'>HST</option><option value='Iceland'>Iceland</option><option value='Indian/Antananarivo'>Indian/Antananarivo</option><option value='Indian/Chagos'>Indian/Chagos</option><option value='Indian/Christmas'>Indian/Christmas</option><option value='Indian/Cocos'>Indian/Cocos</option><option value='Indian/Comoro'>Indian/Comoro</option><option value='Indian/Kerguelen'>Indian/Kerguelen</option><option value='Indian/Mahe'>Indian/Mahe</option><option value='Indian/Maldives'>Indian/Maldives</option><option value='Indian/Mauritius'>Indian/Mauritius</option><option value='Indian/Mayotte'>Indian/Mayotte</option><option value='Indian/Reunion'>Indian/Reunion</option><option value='Iran'>Iran</option><option value='Israel'>Israel</option><option value='Jamaica'>Jamaica</option><option value='Japan'>Japan</option><option value='Kwajalein'>Kwajalein</option><option value='Libya'>Libya</option><option value='MET'>MET</option><option value='Mexico/BajaNorte'>Mexico/BajaNorte</option><option value='Mexico/BajaSur'>Mexico/BajaSur</option><option value='Mexico/General'>Mexico/General</option><option value='MST'>MST</option><option value='MST7MDT'>MST7MDT</option><option value='Navajo'>Navajo</option><option value='NZ'>NZ</option><option value='NZ-CHAT'>NZ-CHAT</option><option value='Pacific/Apia'>Pacific/Apia</option><option value='Pacific/Auckland'>Pacific/Auckland</option><option value='Pacific/Bougainville'>Pacific/Bougainville</option><option value='Pacific/Chatham'>Pacific/Chatham</option><option value='Pacific/Chuuk'>Pacific/Chuuk</option><option value='Pacific/Easter'>Pacific/Easter</option><option value='Pacific/Efate'>Pacific/Efate</option><option value='Pacific/Enderbury'>Pacific/Enderbury</option><option value='Pacific/Fakaofo'>Pacific/Fakaofo</option><option value='Pacific/Fiji'>Pacific/Fiji</option><option value='Pacific/Funafuti'>Pacific/Funafuti</option><option value='Pacific/Galapagos'>Pacific/Galapagos</option><option value='Pacific/Gambier'>Pacific/Gambier</option><option value='Pacific/Guadalcanal'>Pacific/Guadalcanal</option><option value='Pacific/Guam'>Pacific/Guam</option><option value='Pacific/Honolulu'>Pacific/Honolulu</option><option value='Pacific/Johnston'>Pacific/Johnston</option><option value='Pacific/Kanton'>Pacific/Kanton</option><option value='Pacific/Kiritimati'>Pacific/Kiritimati</option><option value='Pacific/Kosrae'>Pacific/Kosrae</option><option value='Pacific/Kwajalein'>Pacific/Kwajalein</option><option value='Pacific/Majuro'>Pacific/Majuro</option><option value='Pacific/Marquesas'>Pacific/Marquesas</option><option value='Pacific/Midway'>Pacific/Midway</option><option value='Pacific/Nauru'>Pacific/Nauru</option><option value='Pacific/Niue'>Pacific/Niue</option><option value='Pacific/Norfolk'>Pacific/Norfolk</option><option value='Pacific/Noumea'>Pacific/Noumea</option><option value='Pacific/Pago_Pago'>Pacific/Pago_Pago</option><option value='Pacific/Palau'>Pacific/Palau</option><option value='Pacific/Pitcairn'>Pacific/Pitcairn</option><option value='Pacific/Pohnpei'>Pacific/Pohnpei</option><option value='Pacific/Ponape'>Pacific/Ponape</option><option value='Pacific/Port_Moresby'>Pacific/Port_Moresby</option><option value='Pacific/Rarotonga'>Pacific/Rarotonga</option><option value='Pacific/Saipan'>Pacific/Saipan</option><option value='Pacific/Samoa'>Pacific/Samoa</option><option value='Pacific/Tahiti'>Pacific/Tahiti</option><option value='Pacific/Tarawa'>Pacific/Tarawa</option><option value='Pacific/Tongatapu'>Pacific/Tongatapu</option><option value='Pacific/Truk'>Pacific/Truk</option><option value='Pacific/Wake'>Pacific/Wake</option><option value='Pacific/Wallis'>Pacific/Wallis</option><option value='Pacific/Yap'>Pacific/Yap</option><option value='Poland'>Poland</option><option value='Portugal'>Portugal</option><option value='PRC'>PRC</option><option value='PST8PDT'>PST8PDT</option><option value='ROC'>ROC</option><option value='ROK'>ROK</option><option value='Singapore'>Singapore</option><option value='Turkey'>Turkey</option><option value='UCT'>UCT</option><option value='Universal'>Universal</option><option value='US/Alaska'>US/Alaska</option><option value='US/Aleutian'>US/Aleutian</option><option value='US/Arizona'>US/Arizona</option><option value='US/Central'>US/Central</option><option value='US/East-Indiana'>US/East-Indiana</option><option value='US/Eastern'>US/Eastern</option><option value='US/Hawaii'>US/Hawaii</option><option value='US/Indiana-Starke'>US/Indiana-Starke</option><option value='US/Michigan'>US/Michigan</option><option value='US/Mountain'>US/Mountain</option><option value='US/Pacific'>US/Pacific</option><option value='US/Samoa'>US/Samoa</option><option value='UTC'>UTC</option><option value='W-SU'>W-SU</option><option value='WET'>WET</option><option value='Zulu '>Zulu</option></select></div><br/><div class='form-inline'><label for='clockcolor'>Select the clock colour:</label><input type='color' id='clockcolor' name='clockcolor' value='#FFFFFF'></div><br/><div class='form-inline'><label for='brightness'>Select the clock brightness:</label><br/><input type='range' id='brightness' name='brightness' value='50' min='0' max='255' oninput='outputbrightness.value = brightness.value'><output id='outputbrightness'>50</output></div><br/><div class='form-inline'>Show seconds at the bottom of the clock?<br/><label for='displaysecondsy'>Yes</label><input type='radio' class='form-inline' name='displayseconds' id='displaysecondsy' value='1' checked='checked'/>&nbsp;&nbsp;&nbsp;<label for='displaysecondsn'>No</label><input type='radio' class='form-inline' name='displayseconds' id='displaysecondsn' value='0' /></div><br/><div class='form-inlne'>Autodim the clock display between 10pm and 7am ?<br/><label for='displayautodimy'>Yes</label><input type='radio' class='form-inline' name='displayautodim' value='1' id='displayautodimy'/>&nbsp;&nbsp;&nbsp;<label for='displayautodimn'>No</label><input type='radio' class='form-inline' name='displayautodim' value='0' checked='checked' id='displayautodimn'/></div><br/><div class='form-inline'><label for='dimstart'>Select the hour when the clock dims:</label><br/><input type='range' id='dimstart' name='dimstart' value='22' min='0' max='23' oninput='dimstartvalue.value = this.value'><output id='dimstartvalue'>22</output></div><br/><div class='form-inline'><label for='dimend'>Select the hour when the clock returns to normal brightness:</label><br/><input type='range' id='dimend' name='dimend' value='7' min='0' max='23' oninput='dimendvalue.value = this.value'><output id='dimendvalue'>7</output></div><br/><div class='form-inline'>Show animations ?<br/><label for='displayanimationy'>Yes</label><input type='radio' class='form-inline' name='displayanimation' value='1' checked='checked' id='displayanimationy'/>&nbsp;&nbsp;&nbsp;<label for='displayanimationn'>No</label><input type='radio' class='form-inline' name='displayanimation' value='0' id='displayanimationn'/></div><br/><button type='submit'>Save</button></form></main></body></html>"));
  }
}


void setup() {
  Serial.begin(115200);

  EEPROM.begin(sizeof(struct settings));



  matrix.begin();
  matrix.setTextWrap(false);
  matrix.setBrightness(50);

  pinMode(buttonPin, INPUT);

  empty_lights_array();

  strip.begin();           // INITIALIZE NeoPixel strip object (REQUIRED)
  strip.show();            // Turn OFF all pixels ASAP
  strip.setBrightness(brightness); // Set BRIGHTNESS to about 1/5 (max = 255)


  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount file system");
    if (!SPIFFS.format()) {
      Serial.println("Formatting failed too");
      return;
    }
    else {
      if (!SPIFFS.begin()) {
        Serial.println("Failed to mount file system after formatting");
        return;
      }
    }
  }

  if (!loadConfig()) {
    Serial.println("Failed to load config");
    //set the clock to white
    clockcolor = strip.Color(255, 255, 255);
    strip.setBrightness(brightness);
  } else {
    Serial.println("Config loaded");
  }
  clockcolor = strip.Color(R, G, B);


  //for testing
  //bootmode = 0;
  Serial.println("bootmode is");
  Serial.println(bootmode);


  if (bootmode == 0) {
    //wifi config settings
    EEPROM.get( 0, user_wifi );
    Serial.println("Made it to bootmode 0");
    WiFi.mode(WIFI_STA);
    Serial.println("Wifi details entered");
    //Serial.println(user_wifi.ssid);
    //Serial.println(user_wifi.password);
    WiFi.begin(user_wifi.ssid, user_wifi.password);


    byte tries = 0;
    while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      Serial.println("Trying to connect to wifi for up to 30 times");
      if (tries++ > 30) {
        Serial.println("Failed to connect - making an access point so config can be re/entered - this will take 30ish seconds");
        WiFi.mode(WIFI_AP);
        WiFi.softAP("Clock Settings 192.168.4.1", "thirdstroke");
        mode = 1;
        break;
      }
    }


    //get time
    setInterval(600);
    waitForSync(30);
    Serial.println();
    Serial.println("UTC:             " + UTC.dateTime());

    myTZ.setLocation(mylocation);
    Serial.print(F("Time in chosen location:     "));
    Serial.println(myTZ.dateTime());

    Serial.println("Clock should be running now");


  }

  if (bootmode == 1) {
    WiFi.begin();
    WiFi.mode(WIFI_AP);
    WiFi.softAP("Clock Settings 192.168.4.1", "thirdstroke");
    mode = 999;
  }






  server.on("/",  handlePortal);
  server.begin();









  // textanim3();
}




void loop() {
  server.handleClient();
  events();


  //normal clock mode
  if (mode == 0) {



    buttonpressedtype =  checkButton();

    if (displayautodim == 1) {
      if (clockhour >= dimstart || clockhour < dimend) {
        strip.setBrightness(10);
      } else {
        strip.setBrightness(brightness);
      }
    }



    if (displayanimation == 1) {
      if (clockminute == 0 && clockseconds == 0) {
        //do an animation, but which one
        randNumber = random(1, 4);
        switch (randNumber) {
          case 1:
            textanim();
            break;
          case 2:
            textanim();
            break;
          case 3:
            textanim();
            break;
        }
      }
    }


    //if longhold move mode on
    if (buttonpressedtype == 4) {
      mode = 1;
      Serial.println("Mode change to");
      Serial.println(mode);
    }


    empty_lights_array();
    strip.clear();
    pixelcolor = strip.Color(0, 0, 0);
    for (int i = 0; i < strip.numPixels(); i++) { // For each pixel in strip...
      strip.setPixelColor(i, pixelcolor);        //  Set pixel's color (in RAM)
    }
    strip.show();

    fill_lights_array();
    strip.clear();

    for (int i = 0; i < strip.numPixels(); i++) { // For each pixel in strip...
      if (lights[i] == 1) {
        //strip.setPixelColor(i, clockcolor);        //  Set pixel's color (in RAM)
        strip.setPixelColor(MAX_IDX - i, clockcolor);
      }
    }
    strip.show();                          //  Update strip to match
    //} else {
    // //too hot turn the LEDS off
    // strip.clear();
    // pixelcolor = strip.Color(0, 0, 0);
    //for (int i = 0; i < strip.numPixels(); i++) { // For each pixel in strip...
    // strip.setPixelColor(i, pixelcolor);        //  Set pixel's color (in RAM)
    //}
    ///strip.show();
    //}
  }


  if (mode == 1) {
    //this is factory reset mode;
    factoryReset = 1;
    //sets back config to basic config and enables wifi access point.
    saveConfig();
    ESP.restart();
  }


// Print time & IP address every 5 secs (for user info)
#define INTERVAL 5000
static unsigned int last_time = 0;
if ((millis() - last_time > INTERVAL) || (millis() < last_time)) {
  Serial.println("UTC:             " + UTC.dateTime());
  Serial.print(F("Time in chosen location:     "));
  Serial.println(myTZ.dateTime());
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  last_time = millis();
}


}





/*
  void loop()
  {
  // Get button event and act accordingly
  int b = checkButton();
  if (b == 1) clickEvent();
  if (b == 2) doubleClickEvent();
  if (b == 3) holdEvent();
  if (b == 4) longHoldEvent();
  }

  //=================================================
  // Events to trigger by click and press+hold

*/





int checkButton() {
  int  event = 0;
  // Read the state of the button
  buttonVal = digitalRead(buttonPin);
  /* if (buttonVal == HIGH) {
     Serial.println("HIGH - pressed");
    } else {
     Serial.println("Low - NOT pressed");
    }*/

  // Button pressed down
  if (buttonVal == LOW && buttonLast == HIGH && (millis() - upTime) > debounce) {
    Serial.println("debounce");
    downTime = millis();
    ignoreUp = false;
    waitForUp = false;
    singleOK = true;
    holdEventPast = false;
    longHoldEventPast = false;
    if ((millis() - upTime) < DCgap && DConUp == false && DCwaiting == true) DConUp = true;
    else DConUp = false;
    DCwaiting = false;
  }
  // Button released
  else if (buttonVal == HIGH && buttonLast == LOW && (millis() - downTime) > debounce) {
    if (not ignoreUp) {
      upTime = millis();
      if (DConUp == false) DCwaiting = true;
      else {
        Serial.println("doubleclick");
        event = 2;
        DConUp = false;
        DCwaiting = false;
        singleOK = false;
      }
    }
  }
  // Test for normal click event: DCgap expired
  if ( buttonVal == HIGH && (millis() - upTime) >= DCgap && DCwaiting == true && DConUp == false && singleOK == true) {
    event = 1;
    Serial.println("click");
    DCwaiting = false;
  }
  // Test for hold
  if (buttonVal == LOW && (millis() - downTime) >= holdTime) {
    // Trigger "normal" hold
    if (not holdEventPast) {
      event = 3;
      Serial.println("hold");
      waitForUp = true;
      ignoreUp = true;
      DConUp = false;
      DCwaiting = false;
      //downTime = millis();
      holdEventPast = true;
    }
    // Trigger "long" hold
    if ((millis() - downTime) >= longHoldTime) {
      if (not longHoldEventPast) {
        event = 4;
        Serial.println("longhold");
        longHoldEventPast = true;
      }
    }
  }
  buttonLast = buttonVal;
  return event;
}




void empty_lights_array() {
  //set lights array so all lights off
  for (int i = 0; i <= 255; i++) {
    lights[i] = 0;
  }
}


void fill_lights_array() {

  clockhour = myTZ.hour();
  clockminute = myTZ.minute();
  clockseconds = myTZ.second();


  //  for testing
  //  clockhour = 0;
  //  clockminute = 1;

  //Serial.println(clockhour);
  //Serial.println(clockminute);
  //Serial.println(clockseconds);

  //normal running
  if (mode == 0) {
    //the time is
    lights[0] = 1;
    lights[31] = 1;
    lights[32] = 1;
    lights[64] = 1;
    lights[95] = 1;
    lights[96] = 1;
    lights[127] = 1;
    lights[159] = 1;
    lights[160] = 1;
  }

  //setting the hour
  if (mode == 1) {
    //mode 1
    lights[112] = 1;
    //set
    lights[209] = 1;
    lights[238] = 1;
    lights[241] = 1;
  }

  //setting the minutes
  if (mode == 2) {
    //mode 2
    lights[143] = 1;
    //set minutes
    lights[209] = 1;
    lights[238] = 1;
    lights[241] = 1;
  }


  //setting if the seconds
  if (mode == 3) {
    //mode 3
    lights[144] = 1;
    lights[209] = 1;
    lights[238] = 1;
    lights[241] = 1;
  }

  //setting display seconds on or off
  if (mode == 4) {
    //mode 4
    lights[175] = 1;
    lights[209] = 1;
    lights[238] = 1;
    lights[241] = 1;

    if (displayseconds == 0) {
      //off
      lights[145] = 1;
      lights[174] = 1;
      lights[177] = 1;
    } else {
      //on
      lights[78] = 1;
      lights[81] = 1;
    }

  }

  //setting colour
  if (mode == 5) {
    //mode 5
    lights[176] = 1;
    lights[209] = 1;
    lights[238] = 1;
    lights[241] = 1;
  }

  //setting brightness
  if (mode == 6) {
    //mode 6
    lights[207] = 1;
    lights[209] = 1;
    lights[238] = 1;
    lights[241] = 1;
  }


  //setting autodim
  if (mode == 7) {
    //mode 7
    lights[208] = 1;
    lights[209] = 1;
    lights[238] = 1;
    lights[241] = 1;
    if (displayautodim == 0) {
      //off
      lights[145] = 1;
      lights[174] = 1;
      lights[177] = 1;
    } else {
      //on
      lights[78] = 1;
      lights[81] = 1;
    }
  }


  //setting animation
  if (mode == 8) {
    //mode 8
    lights[239] = 1;
    lights[209] = 1;
    lights[238] = 1;
    lights[241] = 1;
    if (displayanimation == 0) {
      //off
      lights[145] = 1;
      lights[174] = 1;
      lights[177] = 1;
    } else {
      //on
      lights[78] = 1;
      lights[81] = 1;
    }
  }

  //factory reset
  if (mode == 9) {
    //mode 9
    lights[240] = 1;
    lights[209] = 1;
    lights[238] = 1;
    lights[241] = 1;
    //de
    lights[14] = 1;
    lights[17] = 1;
    if (factoryReset == 0) {
      //off
      lights[145] = 1;
      lights[174] = 1;
      lights[177] = 1;
    } else {
      //on
      lights[78] = 1;
      lights[81] = 1;
    }
  }



  int clockhourlookup = 0;
  int readingahead = 0;
  if (clockminute > 30) {
    clockhourlookup = clockhour + 1;
    readingahead = 1;
  } else {
    clockhourlookup = clockhour;
  }

  if (clockhourlookup == 24) {
    clockhourlookup = 0;
  }


  if (mode == 0 || mode == 1) {
    switch (clockhourlookup) {
      case 0:
        //twelve
        lights[169] = 1;
        lights[182] = 1;
        lights[201] = 1;
        lights[214] = 1;
        lights[233] = 1;
        lights[246] = 1;
        if (readingahead == 1) {
          //at night
          lights[148] = 1;
          lights[171] = 1;
          lights[12] = 1;
          lights[19] = 1;
          lights[44] = 1;
          lights[51] = 1;
          lights[76] = 1;
        } else {
          if (clockminute > 0) {
            //in the morning
            lights[116] = 1;
            lights[139] = 1;
            lights[76] = 1;
            lights[83] = 1;
            lights[108] = 1;
            lights[140] = 1;
            lights[147] = 1;
            lights[172] = 1;
            lights[179] = 1;
            lights[204] = 1;
            lights[211] = 1;
            lights[236] = 1;
          } else {
            //at night
            lights[148] = 1;
            lights[171] = 1;
            lights[12] = 1;
            lights[19] = 1;
            lights[44] = 1;
            lights[51] = 1;
            lights[76] = 1;
          }
        }
        break;
      case 1:
        //one
        lights[40] = 1;
        lights[55] = 1;
        lights[72] = 1;
        //in the morning
        lights[116] = 1;
        lights[139] = 1;
        lights[76] = 1;
        lights[83] = 1;
        lights[108] = 1;
        lights[140] = 1;
        lights[147] = 1;
        lights[172] = 1;
        lights[179] = 1;
        lights[204] = 1;
        lights[211] = 1;
        lights[236] = 1;
        break;
      case 2:
        //two
        lights[8] = 1;
        lights[23] = 1;
        lights[40] = 1;
        //in the morning
        lights[116] = 1;
        lights[139] = 1;
        lights[76] = 1;
        lights[83] = 1;
        lights[108] = 1;
        lights[140] = 1;
        lights[147] = 1;
        lights[172] = 1;
        lights[179] = 1;
        lights[204] = 1;
        lights[211] = 1;
        lights[236] = 1;
        break;
      case 3:
        //three
        lights[86] = 1;
        lights[105] = 1;
        lights[118] = 1;
        lights[137] = 1;
        lights[150] = 1;
        //in the morning
        lights[116] = 1;
        lights[139] = 1;
        lights[76] = 1;
        lights[83] = 1;
        lights[108] = 1;
        lights[140] = 1;
        lights[147] = 1;
        lights[172] = 1;
        lights[179] = 1;
        lights[204] = 1;
        lights[211] = 1;
        lights[236] = 1;
        break;
      case 4:
        //four
        lights[10] = 1;
        lights[21] = 1;
        lights[42] = 1;
        lights[53] = 1;
        //in the morning
        lights[116] = 1;
        lights[139] = 1;
        lights[76] = 1;
        lights[83] = 1;
        lights[108] = 1;
        lights[140] = 1;
        lights[147] = 1;
        lights[172] = 1;
        lights[179] = 1;
        lights[204] = 1;
        lights[211] = 1;
        lights[236] = 1;
        break;
      case 5:
        //five
        lights[85] = 1;
        lights[106] = 1;
        lights[117] = 1;
        lights[138] = 1;
        //in the morning
        lights[116] = 1;
        lights[139] = 1;
        lights[76] = 1;
        lights[83] = 1;
        lights[108] = 1;
        lights[140] = 1;
        lights[147] = 1;
        lights[172] = 1;
        lights[179] = 1;
        lights[204] = 1;
        lights[211] = 1;
        lights[236] = 1;
        break;
      case 6:
        //six
        lights[215] = 1;
        lights[232] = 1;
        lights[247] = 1;
        //in the morning
        lights[116] = 1;
        lights[139] = 1;
        lights[76] = 1;
        lights[83] = 1;
        lights[108] = 1;
        lights[140] = 1;
        lights[147] = 1;
        lights[172] = 1;
        lights[179] = 1;
        lights[204] = 1;
        lights[211] = 1;
        lights[236] = 1;
        break;
      case 7:
        //seven
        lights[9] = 1;
        lights[22] = 1;
        lights[41] = 1;
        lights[54] = 1;
        lights[73] = 1;
        //in the morning
        lights[116] = 1;
        lights[139] = 1;
        lights[76] = 1;
        lights[83] = 1;
        lights[108] = 1;
        lights[140] = 1;
        lights[147] = 1;
        lights[172] = 1;
        lights[179] = 1;
        lights[204] = 1;
        lights[211] = 1;
        lights[236] = 1;
        break;
      case 8:
        //eight
        lights[138] = 1;
        lights[149] = 1;
        lights[170] = 1;
        lights[181] = 1;
        lights[202] = 1;
        //in the morning
        lights[116] = 1;
        lights[139] = 1;
        lights[76] = 1;
        lights[83] = 1;
        lights[108] = 1;
        lights[140] = 1;
        lights[147] = 1;
        lights[172] = 1;
        lights[179] = 1;
        lights[204] = 1;
        lights[211] = 1;
        lights[236] = 1;
        break;
      case 9:
        //nine
        lights[151] = 1;
        lights[168] = 1;
        lights[183] = 1;
        lights[200] = 1;
        //in the morning
        lights[116] = 1;
        lights[139] = 1;
        lights[76] = 1;
        lights[83] = 1;
        lights[108] = 1;
        lights[140] = 1;
        lights[147] = 1;
        lights[172] = 1;
        lights[179] = 1;
        lights[204] = 1;
        lights[211] = 1;
        lights[236] = 1;
        break;
      case 10:
        //ten
        lights[202] = 1;
        lights[213] = 1;
        lights[234] = 1;
        //in the morning
        lights[116] = 1;
        lights[139] = 1;
        lights[76] = 1;
        lights[83] = 1;
        lights[108] = 1;
        lights[140] = 1;
        lights[147] = 1;
        lights[172] = 1;
        lights[179] = 1;
        lights[204] = 1;
        lights[211] = 1;
        lights[236] = 1;
        break;
      case 11:
        //eleven
        lights[72] = 1;
        lights[87] = 1;
        lights[104] = 1;
        lights[119] = 1;
        lights[136] = 1;
        lights[151] = 1;
        //in the morning
        lights[116] = 1;
        lights[139] = 1;
        lights[76] = 1;
        lights[83] = 1;
        lights[108] = 1;
        lights[140] = 1;
        lights[147] = 1;
        lights[172] = 1;
        lights[179] = 1;
        lights[204] = 1;
        lights[211] = 1;
        lights[236] = 1;
        break;
      case 12:
        //twelve
        lights[169] = 1;
        lights[182] = 1;
        lights[201] = 1;
        lights[214] = 1;
        lights[233] = 1;
        lights[246] = 1;
        if (readingahead == 1) {
          //in the morning
          lights[116] = 1;
          lights[139] = 1;
          lights[76] = 1;
          lights[83] = 1;
          lights[108] = 1;
          lights[140] = 1;
          lights[147] = 1;
          lights[172] = 1;
          lights[179] = 1;
          lights[204] = 1;
          lights[211] = 1;
          lights[236] = 1;
        } else {
          //in the afternoon
          lights[116] = 1;
          lights[139] = 1;
          lights[76] = 1;
          lights[83] = 1;
          lights[108] = 1;
          lights[114] = 1;
          lights[141] = 1;
          lights[146] = 1;
          lights[173] = 1;
          lights[178] = 1;
          lights[205] = 1;
          lights[210] = 1;
          lights[237] = 1;
          lights[242] = 1;
        }
        break;
      case 13:
        //one
        lights[40] = 1;
        lights[55] = 1;
        lights[72] = 1;
        //in the afternoon
        lights[116] = 1;
        lights[139] = 1;
        lights[76] = 1;
        lights[83] = 1;
        lights[108] = 1;
        lights[114] = 1;
        lights[141] = 1;
        lights[146] = 1;
        lights[173] = 1;
        lights[178] = 1;
        lights[205] = 1;
        lights[210] = 1;
        lights[237] = 1;
        lights[242] = 1;
        break;
      case 14:
        //two
        lights[8] = 1;
        lights[23] = 1;
        lights[40] = 1;
        //in the afternoon
        lights[116] = 1;
        lights[139] = 1;
        lights[76] = 1;
        lights[83] = 1;
        lights[108] = 1;
        lights[114] = 1;
        lights[141] = 1;
        lights[146] = 1;
        lights[173] = 1;
        lights[178] = 1;
        lights[205] = 1;
        lights[210] = 1;
        lights[237] = 1;
        lights[242] = 1;
        break;
      case 15:
        //three
        lights[86] = 1;
        lights[105] = 1;
        lights[118] = 1;
        lights[137] = 1;
        lights[150] = 1;
        //in the afternoon
        lights[116] = 1;
        lights[139] = 1;
        lights[76] = 1;
        lights[83] = 1;
        lights[108] = 1;
        lights[114] = 1;
        lights[141] = 1;
        lights[146] = 1;
        lights[173] = 1;
        lights[178] = 1;
        lights[205] = 1;
        lights[210] = 1;
        lights[237] = 1;
        lights[242] = 1;
        break;
      case 16:
        //four
        lights[10] = 1;
        lights[21] = 1;
        lights[42] = 1;
        lights[53] = 1;
        //in the afternoon
        lights[116] = 1;
        lights[139] = 1;
        lights[76] = 1;
        lights[83] = 1;
        lights[108] = 1;
        lights[114] = 1;
        lights[141] = 1;
        lights[146] = 1;
        lights[173] = 1;
        lights[178] = 1;
        lights[205] = 1;
        lights[210] = 1;
        lights[237] = 1;
        lights[242] = 1;
        break;
      case 17:
        //five
        lights[85] = 1;
        lights[106] = 1;
        lights[117] = 1;
        lights[138] = 1;
        //in the afternoon
        lights[116] = 1;
        lights[139] = 1;
        lights[76] = 1;
        lights[83] = 1;
        lights[108] = 1;
        lights[114] = 1;
        lights[141] = 1;
        lights[146] = 1;
        lights[173] = 1;
        lights[178] = 1;
        lights[205] = 1;
        lights[210] = 1;
        lights[237] = 1;
        lights[242] = 1;
        break;
      case 18:
        //six
        lights[215] = 1;
        lights[232] = 1;
        lights[247] = 1;
        //in the evening
        lights[116] = 1;
        lights[139] = 1;
        lights[76] = 1;
        lights[83] = 1;
        lights[108] = 1;
        lights[13] = 1;
        lights[18] = 1;
        lights[45] = 1;
        lights[50] = 1;
        lights[77] = 1;
        lights[82] = 1;
        lights[109] = 1;
        break;
      case 19:
        //seven
        lights[9] = 1;
        lights[22] = 1;
        lights[41] = 1;
        lights[54] = 1;
        lights[73] = 1;
        //in the evening
        lights[116] = 1;
        lights[139] = 1;
        lights[76] = 1;
        lights[83] = 1;
        lights[108] = 1;
        lights[13] = 1;
        lights[18] = 1;
        lights[45] = 1;
        lights[50] = 1;
        lights[77] = 1;
        lights[82] = 1;
        lights[109] = 1;
        break;
      case 20:
        //eight
        lights[138] = 1;
        lights[149] = 1;
        lights[170] = 1;
        lights[181] = 1;
        lights[202] = 1;
        //in the evening
        lights[116] = 1;
        lights[139] = 1;
        lights[76] = 1;
        lights[83] = 1;
        lights[108] = 1;
        lights[13] = 1;
        lights[18] = 1;
        lights[45] = 1;
        lights[50] = 1;
        lights[77] = 1;
        lights[82] = 1;
        lights[109] = 1;
        break;
      case 21:
        //nine
        lights[151] = 1;
        lights[168] = 1;
        lights[183] = 1;
        lights[200] = 1;
        //at night
        lights[148] = 1;
        lights[171] = 1;
        lights[12] = 1;
        lights[19] = 1;
        lights[44] = 1;
        lights[51] = 1;
        lights[76] = 1;
        break;
      case 22:
        //ten
        lights[202] = 1;
        lights[213] = 1;
        lights[234] = 1;
        //at night
        lights[148] = 1;
        lights[171] = 1;
        lights[12] = 1;
        lights[19] = 1;
        lights[44] = 1;
        lights[51] = 1;
        lights[76] = 1;
        break;
      case 23:
        //eleven
        lights[72] = 1;
        lights[87] = 1;
        lights[104] = 1;
        lights[119] = 1;
        lights[136] = 1;
        lights[151] = 1;
        //at night
        lights[148] = 1;
        lights[171] = 1;
        lights[12] = 1;
        lights[19] = 1;
        lights[44] = 1;
        lights[51] = 1;
        lights[76] = 1;
        break;
    }
  }

  if (mode == 0 || mode == 2) {

    switch (clockminute) {
      case 0:
        //oclock
        lights[11] = 1;
        lights[20] = 1;
        lights[43] = 1;
        lights[52] = 1;
        lights[75] = 1;
        lights[84] = 1;
        break;
      case 1:
        //one
        lights[194] = 1;
        lights[221] = 1;
        lights[226] = 1;
        //minute past
        lights[7] = 1;
        lights[24] = 1;
        lights[39] = 1;
        lights[56] = 1;
        lights[71] = 1;
        lights[88] = 1;
        lights[135] = 1;
        lights[152] = 1;
        lights[167] = 1;
        lights[184] = 1;
        break;
      case 2:
        //two
        lights[162] = 1;
        lights[189] = 1;
        lights[194] = 1;
        //minutes past
        lights[7] = 1;
        lights[24] = 1;
        lights[39] = 1;
        lights[56] = 1;
        lights[71] = 1;
        lights[88] = 1;
        lights[103] = 1;
        lights[135] = 1;
        lights[152] = 1;
        lights[167] = 1;
        lights[184] = 1;
        break;
      case 3:
        //three
        lights[185] = 1;
        lights[198] = 1;
        lights[217] = 1;
        lights[230] = 1;
        lights[249] = 1;
        //minutes past
        lights[7] = 1;
        lights[24] = 1;
        lights[39] = 1;
        lights[56] = 1;
        lights[71] = 1;
        lights[88] = 1;
        lights[103] = 1;
        lights[135] = 1;
        lights[152] = 1;
        lights[167] = 1;
        lights[184] = 1;
        break;
      case 4:
        //four
        lights[5] = 1;
        lights[26] = 1;
        lights[37] = 1;
        lights[58] = 1;
        //minutes past
        lights[7] = 1;
        lights[24] = 1;
        lights[39] = 1;
        lights[56] = 1;
        lights[71] = 1;
        lights[88] = 1;
        lights[103] = 1;
        lights[135] = 1;
        lights[152] = 1;
        lights[167] = 1;
        lights[184] = 1;
        break;
      case 5:
        //five
        lights[131] = 1;
        lights[156] = 1;
        lights[163] = 1;
        lights[188] = 1;
        //past
        lights[135] = 1;
        lights[152] = 1;
        lights[167] = 1;
        lights[184] = 1;
        break;
      case 6:
        //six
        lights[61] = 1;
        lights[66] = 1;
        lights[93] = 1;
        //minutes past
        lights[7] = 1;
        lights[24] = 1;
        lights[39] = 1;
        lights[56] = 1;
        lights[71] = 1;
        lights[88] = 1;
        lights[103] = 1;
        lights[135] = 1;
        lights[152] = 1;
        lights[167] = 1;
        lights[184] = 1;
        break;
      case 7:
        //seven
        lights[4] = 1;
        lights[27] = 1;
        lights[36] = 1;
        lights[59] = 1;
        lights[68] = 1;
        //minutes past
        lights[7] = 1;
        lights[24] = 1;
        lights[39] = 1;
        lights[56] = 1;
        lights[71] = 1;
        lights[88] = 1;
        lights[103] = 1;
        lights[135] = 1;
        lights[152] = 1;
        lights[167] = 1;
        lights[184] = 1;
        break;
      case 8:
        //eight
        lights[3] = 1;
        lights[28] = 1;
        lights[35] = 1;
        lights[60] = 1;
        lights[67] = 1;
        //minutes past
        lights[7] = 1;
        lights[24] = 1;
        lights[39] = 1;
        lights[56] = 1;
        lights[71] = 1;
        lights[88] = 1;
        lights[103] = 1;
        lights[135] = 1;
        lights[152] = 1;
        lights[167] = 1;
        lights[184] = 1;
        break;
      case 9:
        //nine
        lights[132] = 1;
        lights[155] = 1;
        lights[164] = 1;
        lights[187] = 1;
        //minutes past
        lights[7] = 1;
        lights[24] = 1;
        lights[39] = 1;
        lights[56] = 1;
        lights[71] = 1;
        lights[88] = 1;
        lights[103] = 1;
        lights[135] = 1;
        lights[152] = 1;
        lights[167] = 1;
        lights[184] = 1;
        break;
      case 10:
        //ten
        lights[2] = 1;
        lights[29] = 1;
        lights[34] = 1;
        //past
        lights[135] = 1;
        lights[152] = 1;
        lights[167] = 1;
        lights[184] = 1;
        break;
      case 11:
        //eleven
        lights[89] = 1;
        lights[102] = 1;
        lights[121] = 1;
        lights[134] = 1;
        lights[153] = 1;
        lights[166] = 1;
        //minutes past
        lights[7] = 1;
        lights[24] = 1;
        lights[39] = 1;
        lights[56] = 1;
        lights[71] = 1;
        lights[88] = 1;
        lights[103] = 1;
        lights[135] = 1;
        lights[152] = 1;
        lights[167] = 1;
        lights[184] = 1;
        break;
      case 12:
        //twelve
        lights[6] = 1;
        lights[25] = 1;
        lights[38] = 1;
        lights[57] = 1;
        lights[70] = 1;
        lights[89] = 1;
        //minutes past
        lights[7] = 1;
        lights[24] = 1;
        lights[39] = 1;
        lights[56] = 1;
        lights[71] = 1;
        lights[88] = 1;
        lights[103] = 1;
        lights[135] = 1;
        lights[152] = 1;
        lights[167] = 1;
        lights[184] = 1;
        break;
      case 13:
        //thirteen
        lights[133] = 1;
        lights[154] = 1;
        lights[165] = 1;
        lights[186] = 1;
        lights[197] = 1;
        lights[218] = 1;
        lights[229] = 1;
        lights[250] = 1;
        //minutes past
        lights[7] = 1;
        lights[24] = 1;
        lights[39] = 1;
        lights[56] = 1;
        lights[71] = 1;
        lights[88] = 1;
        lights[103] = 1;
        lights[135] = 1;
        lights[152] = 1;
        lights[167] = 1;
        lights[184] = 1;
        break;
      case 14:
        //fourteen
        lights[5] = 1;
        lights[26] = 1;
        lights[37] = 1;
        lights[58] = 1;
        lights[69] = 1;
        lights[90] = 1;
        lights[101] = 1;
        lights[122] = 1;
        //minutes past
        lights[7] = 1;
        lights[24] = 1;
        lights[39] = 1;
        lights[56] = 1;
        lights[71] = 1;
        lights[88] = 1;
        lights[103] = 1;
        lights[135] = 1;
        lights[152] = 1;
        lights[167] = 1;
        lights[184] = 1;
        break;
      case 15:
        //quarter
        lights[1] = 1;
        lights[30] = 1;
        lights[33] = 1;
        lights[62] = 1;
        lights[65] = 1;
        lights[94] = 1;
        lights[97] = 1;
        //past
        lights[135] = 1;
        lights[152] = 1;
        lights[167] = 1;
        lights[184] = 1;
        break;
      case 16:
        //sixteen
        lights[61] = 1;
        lights[66] = 1;
        lights[93] = 1;
        lights[98] = 1;
        lights[125] = 1;
        lights[130] = 1;
        lights[157] = 1;
        //minutes past
        lights[7] = 1;
        lights[24] = 1;
        lights[39] = 1;
        lights[56] = 1;
        lights[71] = 1;
        lights[88] = 1;
        lights[103] = 1;
        lights[135] = 1;
        lights[152] = 1;
        lights[167] = 1;
        lights[184] = 1;
        break;
      case 17:
        //seventeen
        lights[4] = 1;
        lights[27] = 1;
        lights[36] = 1;
        lights[59] = 1;
        lights[68] = 1;
        lights[91] = 1;
        lights[100] = 1;
        lights[123] = 1;
        lights[132] = 1;
        //minutes past
        lights[7] = 1;
        lights[24] = 1;
        lights[39] = 1;
        lights[56] = 1;
        lights[71] = 1;
        lights[88] = 1;
        lights[103] = 1;
        lights[135] = 1;
        lights[152] = 1;
        lights[167] = 1;
        lights[184] = 1;
        break;
      case 18:
        //eighteen
        lights[3] = 1;
        lights[28] = 1;
        lights[35] = 1;
        lights[60] = 1;
        lights[67] = 1;
        lights[92] = 1;
        lights[99] = 1;
        lights[124] = 1;
        //minutes past
        lights[7] = 1;
        lights[24] = 1;
        lights[39] = 1;
        lights[56] = 1;
        lights[71] = 1;
        lights[88] = 1;
        lights[103] = 1;
        lights[135] = 1;
        lights[152] = 1;
        lights[167] = 1;
        lights[184] = 1;
        break;
      case 19:
        //nineteen
        lights[132] = 1;
        lights[155] = 1;
        lights[164] = 1;
        lights[187] = 1;
        lights[196] = 1;
        lights[219] = 1;
        lights[228] = 1;
        lights[251] = 1;
        //minutes past
        lights[7] = 1;
        lights[24] = 1;
        lights[39] = 1;
        lights[56] = 1;
        lights[71] = 1;
        lights[88] = 1;
        lights[103] = 1;
        lights[135] = 1;
        lights[152] = 1;
        lights[167] = 1;
        lights[184] = 1;
        break;
      case 20:
        //twenty
        lights[126] = 1;
        lights[129] = 1;
        lights[158] = 1;
        lights[161] = 1;
        lights[190] = 1;
        lights[193] = 1;
        //past
        lights[135] = 1;
        lights[152] = 1;
        lights[167] = 1;
        lights[184] = 1;
        break;
      case 21:
        //twenty
        lights[126] = 1;
        lights[129] = 1;
        lights[158] = 1;
        lights[161] = 1;
        lights[190] = 1;
        lights[193] = 1;
        //one
        lights[194] = 1;
        lights[221] = 1;
        lights[226] = 1;
        //minutes past
        lights[7] = 1;
        lights[24] = 1;
        lights[39] = 1;
        lights[56] = 1;
        lights[71] = 1;
        lights[88] = 1;
        lights[103] = 1;
        lights[135] = 1;
        lights[152] = 1;
        lights[167] = 1;
        lights[184] = 1;
        break;
      case 22:
        //twenty
        lights[126] = 1;
        lights[129] = 1;
        lights[158] = 1;
        lights[161] = 1;
        lights[190] = 1;
        lights[193] = 1;
        //two
        lights[162] = 1;
        lights[189] = 1;
        lights[194] = 1;
        //minutes past
        lights[7] = 1;
        lights[24] = 1;
        lights[39] = 1;
        lights[56] = 1;
        lights[71] = 1;
        lights[88] = 1;
        lights[103] = 1;
        lights[135] = 1;
        lights[152] = 1;
        lights[167] = 1;
        lights[184] = 1;
        break;
      case 23:
        //twenty
        lights[126] = 1;
        lights[129] = 1;
        lights[158] = 1;
        lights[161] = 1;
        lights[190] = 1;
        lights[193] = 1;
        //three
        lights[185] = 1;
        lights[198] = 1;
        lights[217] = 1;
        lights[230] = 1;
        lights[249] = 1;
        //minutes past
        lights[7] = 1;
        lights[24] = 1;
        lights[39] = 1;
        lights[56] = 1;
        lights[71] = 1;
        lights[88] = 1;
        lights[103] = 1;
        lights[135] = 1;
        lights[152] = 1;
        lights[167] = 1;
        lights[184] = 1;
        break;
      case 24:
        //twenty
        lights[126] = 1;
        lights[129] = 1;
        lights[158] = 1;
        lights[161] = 1;
        lights[190] = 1;
        lights[193] = 1;
        //four
        lights[5] = 1;
        lights[26] = 1;
        lights[37] = 1;
        lights[58] = 1;
        //minutes past
        lights[7] = 1;
        lights[24] = 1;
        lights[39] = 1;
        lights[56] = 1;
        lights[71] = 1;
        lights[88] = 1;
        lights[103] = 1;
        lights[135] = 1;
        lights[152] = 1;
        lights[167] = 1;
        lights[184] = 1;
        break;
      case 25:
        //twenty
        lights[126] = 1;
        lights[129] = 1;
        lights[158] = 1;
        lights[161] = 1;
        lights[190] = 1;
        lights[193] = 1;
        //five
        lights[131] = 1;
        lights[156] = 1;
        lights[163] = 1;
        lights[188] = 1;
        //past
        lights[135] = 1;
        lights[152] = 1;
        lights[167] = 1;
        lights[184] = 1;
        break;
      case 26:
        //twenty
        lights[126] = 1;
        lights[129] = 1;
        lights[158] = 1;
        lights[161] = 1;
        lights[190] = 1;
        lights[193] = 1;
        //six
        lights[61] = 1;
        lights[66] = 1;
        lights[93] = 1;
        //minutes past
        lights[7] = 1;
        lights[24] = 1;
        lights[39] = 1;
        lights[56] = 1;
        lights[71] = 1;
        lights[88] = 1;
        lights[103] = 1;
        lights[135] = 1;
        lights[152] = 1;
        lights[167] = 1;
        lights[184] = 1;
        break;
      case 27:
        //twenty
        lights[126] = 1;
        lights[129] = 1;
        lights[158] = 1;
        lights[161] = 1;
        lights[190] = 1;
        lights[193] = 1;
        //seven
        lights[4] = 1;
        lights[27] = 1;
        lights[36] = 1;
        lights[59] = 1;
        lights[68] = 1;
        //minutes past
        lights[7] = 1;
        lights[24] = 1;
        lights[39] = 1;
        lights[56] = 1;
        lights[71] = 1;
        lights[88] = 1;
        lights[103] = 1;
        lights[135] = 1;
        lights[152] = 1;
        lights[167] = 1;
        lights[184] = 1;
        break;
      case 28:
        //twenty
        lights[126] = 1;
        lights[129] = 1;
        lights[158] = 1;
        lights[161] = 1;
        lights[190] = 1;
        lights[193] = 1;
        //eight
        lights[3] = 1;
        lights[28] = 1;
        lights[35] = 1;
        lights[60] = 1;
        lights[67] = 1;
        //minutes past
        lights[7] = 1;
        lights[24] = 1;
        lights[39] = 1;
        lights[56] = 1;
        lights[71] = 1;
        lights[88] = 1;
        lights[103] = 1;
        lights[135] = 1;
        lights[152] = 1;
        lights[167] = 1;
        lights[184] = 1;
        break;
      case 29:
        //twenty
        lights[126] = 1;
        lights[129] = 1;
        lights[158] = 1;
        lights[161] = 1;
        lights[190] = 1;
        lights[193] = 1;
        //nine
        lights[132] = 1;
        lights[155] = 1;
        lights[164] = 1;
        lights[187] = 1;
        //minutes past
        lights[7] = 1;
        lights[24] = 1;
        lights[39] = 1;
        lights[56] = 1;
        lights[71] = 1;
        lights[88] = 1;
        lights[103] = 1;
        lights[135] = 1;
        lights[152] = 1;
        lights[167] = 1;
        lights[184] = 1;
        break;
      case 30:
        //half
        lights[192] = 1;
        lights[223] = 1;
        lights[224] = 1;
        lights[255] = 1;
        //past
        lights[135] = 1;
        lights[152] = 1;
        lights[167] = 1;
        lights[184] = 1;
        break;
      case 31:
        //twenty
        lights[126] = 1;
        lights[129] = 1;
        lights[158] = 1;
        lights[161] = 1;
        lights[190] = 1;
        lights[193] = 1;
        //nine
        lights[132] = 1;
        lights[155] = 1;
        lights[164] = 1;
        lights[187] = 1;
        //minutes to
        lights[7] = 1;
        lights[24] = 1;
        lights[39] = 1;
        lights[56] = 1;
        lights[71] = 1;
        lights[88] = 1;
        lights[103] = 1;
        lights[184] = 1;
        lights[199] = 1;
        break;
      case 32:
        //twenty
        lights[126] = 1;
        lights[129] = 1;
        lights[158] = 1;
        lights[161] = 1;
        lights[190] = 1;
        lights[193] = 1;
        //eight
        lights[3] = 1;
        lights[28] = 1;
        lights[35] = 1;
        lights[60] = 1;
        lights[67] = 1;
        //minutes to
        lights[7] = 1;
        lights[24] = 1;
        lights[39] = 1;
        lights[56] = 1;
        lights[71] = 1;
        lights[88] = 1;
        lights[103] = 1;
        lights[184] = 1;
        lights[199] = 1;
        break;
      case 33:
        //twenty
        lights[126] = 1;
        lights[129] = 1;
        lights[158] = 1;
        lights[161] = 1;
        lights[190] = 1;
        lights[193] = 1;
        //seven
        lights[4] = 1;
        lights[27] = 1;
        lights[36] = 1;
        lights[59] = 1;
        lights[68] = 1;
        //minutes to
        lights[7] = 1;
        lights[24] = 1;
        lights[39] = 1;
        lights[56] = 1;
        lights[71] = 1;
        lights[88] = 1;
        lights[103] = 1;
        lights[184] = 1;
        lights[199] = 1;
        break;
      case 34:
        //twenty
        lights[126] = 1;
        lights[129] = 1;
        lights[158] = 1;
        lights[161] = 1;
        lights[190] = 1;
        lights[193] = 1;
        //six
        lights[61] = 1;
        lights[66] = 1;
        lights[93] = 1;
        //minutes to
        lights[7] = 1;
        lights[24] = 1;
        lights[39] = 1;
        lights[56] = 1;
        lights[71] = 1;
        lights[88] = 1;
        lights[103] = 1;
        lights[184] = 1;
        lights[199] = 1;
        break;
      case 35:
        //twenty
        lights[126] = 1;
        lights[129] = 1;
        lights[158] = 1;
        lights[161] = 1;
        lights[190] = 1;
        lights[193] = 1;
        //five
        lights[131] = 1;
        lights[156] = 1;
        lights[163] = 1;
        lights[188] = 1;
        //to
        lights[184] = 1;
        lights[199] = 1;
        break;
      case 36:
        //twenty
        lights[126] = 1;
        lights[129] = 1;
        lights[158] = 1;
        lights[161] = 1;
        lights[190] = 1;
        lights[193] = 1;
        //four
        lights[5] = 1;
        lights[26] = 1;
        lights[37] = 1;
        lights[58] = 1;
        //minutes to
        lights[7] = 1;
        lights[24] = 1;
        lights[39] = 1;
        lights[56] = 1;
        lights[71] = 1;
        lights[88] = 1;
        lights[103] = 1;
        lights[184] = 1;
        lights[199] = 1;
        break;
      case 37:
        //twenty
        lights[126] = 1;
        lights[129] = 1;
        lights[158] = 1;
        lights[161] = 1;
        lights[190] = 1;
        lights[193] = 1;
        //three
        lights[185] = 1;
        lights[198] = 1;
        lights[217] = 1;
        lights[230] = 1;
        lights[249] = 1;
        //minutes to
        lights[7] = 1;
        lights[24] = 1;
        lights[39] = 1;
        lights[56] = 1;
        lights[71] = 1;
        lights[88] = 1;
        lights[103] = 1;
        lights[184] = 1;
        lights[199] = 1;
        break;
      case 38:
        //twenty
        lights[126] = 1;
        lights[129] = 1;
        lights[158] = 1;
        lights[161] = 1;
        lights[190] = 1;
        lights[193] = 1;
        //two
        lights[162] = 1;
        lights[189] = 1;
        lights[194] = 1;
        //minutes to
        lights[7] = 1;
        lights[24] = 1;
        lights[39] = 1;
        lights[56] = 1;
        lights[71] = 1;
        lights[88] = 1;
        lights[103] = 1;
        lights[184] = 1;
        lights[199] = 1;
        break;
      case 39:
        //twenty
        lights[126] = 1;
        lights[129] = 1;
        lights[158] = 1;
        lights[161] = 1;
        lights[190] = 1;
        lights[193] = 1;
        //one
        lights[194] = 1;
        lights[221] = 1;
        lights[226] = 1;
        //minutes to
        lights[7] = 1;
        lights[24] = 1;
        lights[39] = 1;
        lights[56] = 1;
        lights[71] = 1;
        lights[88] = 1;
        lights[103] = 1;
        lights[184] = 1;
        lights[199] = 1;
        break;
      case 40:
        //twenty
        lights[126] = 1;
        lights[129] = 1;
        lights[158] = 1;
        lights[161] = 1;
        lights[190] = 1;
        lights[193] = 1;
        //to
        lights[184] = 1;
        lights[199] = 1;
        break;
      case 41:
        //nineteen
        lights[132] = 1;
        lights[155] = 1;
        lights[164] = 1;
        lights[187] = 1;
        lights[196] = 1;
        lights[219] = 1;
        lights[228] = 1;
        lights[251] = 1;
        //minutes to
        lights[7] = 1;
        lights[24] = 1;
        lights[39] = 1;
        lights[56] = 1;
        lights[71] = 1;
        lights[88] = 1;
        lights[103] = 1;
        lights[184] = 1;
        lights[199] = 1;
        break;
      case 42:
        //eighteen
        lights[3] = 1;
        lights[28] = 1;
        lights[35] = 1;
        lights[60] = 1;
        lights[67] = 1;
        lights[92] = 1;
        lights[99] = 1;
        lights[124] = 1;
        //minutes to
        lights[7] = 1;
        lights[24] = 1;
        lights[39] = 1;
        lights[56] = 1;
        lights[71] = 1;
        lights[88] = 1;
        lights[103] = 1;
        lights[184] = 1;
        lights[199] = 1;
        break;
      case 43:
        //seventeen
        lights[4] = 1;
        lights[27] = 1;
        lights[36] = 1;
        lights[59] = 1;
        lights[68] = 1;
        lights[91] = 1;
        lights[100] = 1;
        lights[123] = 1;
        lights[132] = 1;
        //minutes to
        lights[7] = 1;
        lights[24] = 1;
        lights[39] = 1;
        lights[56] = 1;
        lights[71] = 1;
        lights[88] = 1;
        lights[103] = 1;
        lights[184] = 1;
        lights[199] = 1;
        break;
      case 44:
        //sixteen
        lights[61] = 1;
        lights[66] = 1;
        lights[93] = 1;
        lights[98] = 1;
        lights[125] = 1;
        lights[130] = 1;
        lights[157] = 1;
        //minutes to
        lights[7] = 1;
        lights[24] = 1;
        lights[39] = 1;
        lights[56] = 1;
        lights[71] = 1;
        lights[88] = 1;
        lights[103] = 1;
        lights[184] = 1;
        lights[199] = 1;
        break;
      case 45:
        //quarter
        lights[1] = 1;
        lights[30] = 1;
        lights[33] = 1;
        lights[62] = 1;
        lights[65] = 1;
        lights[94] = 1;
        lights[97] = 1;
        //to
        lights[184] = 1;
        lights[199] = 1;
        break;
      case 46:
        //fourteen
        lights[5] = 1;
        lights[26] = 1;
        lights[37] = 1;
        lights[58] = 1;
        lights[69] = 1;
        lights[90] = 1;
        lights[101] = 1;
        lights[122] = 1;
        //minutes to
        lights[7] = 1;
        lights[24] = 1;
        lights[39] = 1;
        lights[56] = 1;
        lights[71] = 1;
        lights[88] = 1;
        lights[103] = 1;
        lights[184] = 1;
        lights[199] = 1;
        break;
      case 47:
        //thirteen
        lights[133] = 1;
        lights[154] = 1;
        lights[165] = 1;
        lights[186] = 1;
        lights[197] = 1;
        lights[218] = 1;
        lights[229] = 1;
        lights[250] = 1;
        //minutes to
        lights[7] = 1;
        lights[24] = 1;
        lights[39] = 1;
        lights[56] = 1;
        lights[71] = 1;
        lights[88] = 1;
        lights[103] = 1;
        lights[184] = 1;
        lights[199] = 1;
        break;
      case 48:
        //twelve
        lights[6] = 1;
        lights[25] = 1;
        lights[38] = 1;
        lights[57] = 1;
        lights[70] = 1;
        lights[89] = 1;
        //minutes to
        lights[7] = 1;
        lights[24] = 1;
        lights[39] = 1;
        lights[56] = 1;
        lights[71] = 1;
        lights[88] = 1;
        lights[103] = 1;
        lights[184] = 1;
        lights[199] = 1;
        break;
      case 49:
        //eleven
        lights[89] = 1;
        lights[102] = 1;
        lights[121] = 1;
        lights[134] = 1;
        lights[153] = 1;
        lights[166] = 1;
        //minutes to
        lights[7] = 1;
        lights[24] = 1;
        lights[39] = 1;
        lights[56] = 1;
        lights[71] = 1;
        lights[88] = 1;
        lights[103] = 1;
        lights[184] = 1;
        lights[199] = 1;
        break;
      case 50:
        //ten
        lights[2] = 1;
        lights[29] = 1;
        lights[34] = 1;
        //to
        lights[184] = 1;
        lights[199] = 1;
        break;
      case 51:
        //nine
        lights[132] = 1;
        lights[155] = 1;
        lights[164] = 1;
        lights[187] = 1;
        //minutes to
        lights[7] = 1;
        lights[24] = 1;
        lights[39] = 1;
        lights[56] = 1;
        lights[71] = 1;
        lights[88] = 1;
        lights[103] = 1;
        lights[184] = 1;
        lights[199] = 1;
        break;
      case 52:
        //eight
        lights[3] = 1;
        lights[28] = 1;
        lights[35] = 1;
        lights[60] = 1;
        lights[67] = 1;
        //minutes to
        lights[7] = 1;
        lights[24] = 1;
        lights[39] = 1;
        lights[56] = 1;
        lights[71] = 1;
        lights[88] = 1;
        lights[103] = 1;
        lights[184] = 1;
        lights[199] = 1;
        break;
      case 53:
        //seven
        lights[4] = 1;
        lights[27] = 1;
        lights[36] = 1;
        lights[59] = 1;
        lights[68] = 1;
        //minutes to
        lights[7] = 1;
        lights[24] = 1;
        lights[39] = 1;
        lights[56] = 1;
        lights[71] = 1;
        lights[88] = 1;
        lights[103] = 1;
        lights[184] = 1;
        lights[199] = 1;
        break;
      case 54:
        //six
        lights[61] = 1;
        lights[66] = 1;
        lights[93] = 1;
        //minutes to
        lights[7] = 1;
        lights[24] = 1;
        lights[39] = 1;
        lights[56] = 1;
        lights[71] = 1;
        lights[88] = 1;
        lights[103] = 1;
        lights[184] = 1;
        lights[199] = 1;
        break;
      case 55:
        //five
        lights[131] = 1;
        lights[156] = 1;
        lights[163] = 1;
        lights[188] = 1;
        //to
        lights[184] = 1;
        lights[199] = 1;
        break;
      case 56:
        //four
        lights[5] = 1;
        lights[26] = 1;
        lights[37] = 1;
        lights[58] = 1;
        //minutes to
        lights[7] = 1;
        lights[24] = 1;
        lights[39] = 1;
        lights[56] = 1;
        lights[71] = 1;
        lights[88] = 1;
        lights[103] = 1;
        lights[184] = 1;
        lights[199] = 1;
        break;
      case 57:
        //three
        lights[185] = 1;
        lights[198] = 1;
        lights[217] = 1;
        lights[230] = 1;
        lights[249] = 1;
        //minutes to
        lights[7] = 1;
        lights[24] = 1;
        lights[39] = 1;
        lights[56] = 1;
        lights[71] = 1;
        lights[88] = 1;
        lights[103] = 1;
        lights[184] = 1;
        lights[199] = 1;
        break;
      case 58:
        //two
        lights[162] = 1;
        lights[189] = 1;
        lights[194] = 1;
        //minutes to
        lights[7] = 1;
        lights[24] = 1;
        lights[39] = 1;
        lights[56] = 1;
        lights[71] = 1;
        lights[88] = 1;
        lights[103] = 1;
        lights[184] = 1;
        lights[199] = 1;
        break;
      case 59:
        //one
        lights[194] = 1;
        lights[221] = 1;
        lights[226] = 1;
        //minute to
        lights[7] = 1;
        lights[24] = 1;
        lights[39] = 1;
        lights[56] = 1;
        lights[71] = 1;
        lights[88] = 1;

        lights[184] = 1;
        lights[199] = 1;
        break;
    }
  }


  if (displayseconds == 1 && mode == 0) {
    switch (clockseconds) {
      case 0:
        lights[15] = 1;
        lights[111] = 1;
        break;
      case 1:
        lights[15] = 1;
        lights[112] = 1;
        break;
      case 2:
        lights[15] = 1;
        lights[143] = 1;
        break;
      case 3:
        lights[15] = 1;
        lights[144] = 1;
        break;
      case 4:
        lights[15] = 1;
        lights[175] = 1;
        break;
      case 5:
        lights[15] = 1;
        lights[176] = 1;
        break;
      case 6:
        lights[15] = 1;
        lights[207] = 1;
        break;
      case 7:
        lights[15] = 1;
        lights[208] = 1;
        break;
      case 8:
        lights[15] = 1;
        lights[239] = 1;
        break;
      case 9:
        lights[15] = 1;
        lights[240] = 1;
        break;
      case 10:
        lights[16] = 1;
        lights[111] = 1;
        break;
      case 11:
        lights[16] = 1;
        lights[112] = 1;
        break;
      case 12:
        lights[16] = 1;
        lights[143] = 1;
        break;
      case 13:
        lights[16] = 1;
        lights[144] = 1;
        break;
      case 14:
        lights[16] = 1;
        lights[175] = 1;
        break;
      case 15:
        lights[16] = 1;
        lights[176] = 1;
        break;
      case 16:
        lights[16] = 1;
        lights[207] = 1;
        break;
      case 17:
        lights[16] = 1;
        lights[208] = 1;
        break;
      case 18:
        lights[16] = 1;
        lights[239] = 1;
        break;
      case 19:
        lights[16] = 1;
        lights[240] = 1;
        break;
      case 20:
        lights[47] = 1;
        lights[111] = 1;
        break;
      case 21:
        lights[47] = 1;
        lights[112] = 1;
        break;
      case 22:
        lights[47] = 1;
        lights[143] = 1;
        break;
      case 23:
        lights[47] = 1;
        lights[144] = 1;
        break;
      case 24:
        lights[47] = 1;
        lights[175] = 1;
        break;
      case 25:
        lights[47] = 1;
        lights[176] = 1;
        break;
      case 26:
        lights[47] = 1;
        lights[207] = 1;
        break;
      case 27:
        lights[47] = 1;
        lights[208] = 1;
        break;
      case 28:
        lights[47] = 1;
        lights[239] = 1;
        break;
      case 29:
        lights[47] = 1;
        lights[240] = 1;
        break;
      case 30:
        lights[48] = 1;
        lights[111] = 1;
        break;
      case 31:
        lights[48] = 1;
        lights[112] = 1;
        break;
      case 32:
        lights[48] = 1;
        lights[143] = 1;
        break;
      case 33:
        lights[48] = 1;
        lights[144] = 1;
        break;
      case 34:
        lights[48] = 1;
        lights[175] = 1;
        break;
      case 35:
        lights[48] = 1;
        lights[176] = 1;
        break;
      case 36:
        lights[48] = 1;
        lights[207] = 1;
        break;
      case 37:
        lights[48] = 1;
        lights[208] = 1;
        break;
      case 38:
        lights[48] = 1;
        lights[239] = 1;
        break;
      case 39:
        lights[48] = 1;
        lights[240] = 1;
        break;
      case 40:
        lights[79] = 1;
        lights[111] = 1;
        break;
      case 41:
        lights[79] = 1;
        lights[112] = 1;
        break;
      case 42:
        lights[79] = 1;
        lights[143] = 1;
        break;
      case 43:
        lights[79] = 1;
        lights[144] = 1;
        break;
      case 44:
        lights[79] = 1;
        lights[175] = 1;
        break;
      case 45:
        lights[79] = 1;
        lights[176] = 1;
        break;
      case 46:
        lights[79] = 1;
        lights[207] = 1;
        break;
      case 47:
        lights[79] = 1;
        lights[208] = 1;
        break;
      case 48:
        lights[79] = 1;
        lights[239] = 1;
        break;
      case 49:
        lights[79] = 1;
        lights[240] = 1;
        break;
      case 50:
        lights[80] = 1;
        lights[111] = 1;
        break;
      case 51:
        lights[80] = 1;
        lights[112] = 1;
        break;
      case 52:
        lights[80] = 1;
        lights[143] = 1;
        break;
      case 53:
        lights[80] = 1;
        lights[144] = 1;
        break;
      case 54:
        lights[80] = 1;
        lights[175] = 1;
        break;
      case 55:
        lights[80] = 1;
        lights[176] = 1;
        break;
      case 56:
        lights[80] = 1;
        lights[207] = 1;
        break;
      case 57:
        lights[80] = 1;
        lights[208] = 1;
        break;
      case 58:
        lights[80] = 1;
        lights[239] = 1;
        break;
      case 59:
        lights[80] = 1;
        lights[240] = 1;
        break;
    }
  }

}


void speedclock() {
  for (clockhour = 0; clockhour < 23; clockhour++) {
    Serial.println(clockhour);
    for (clockminute = 0; clockminute < 60; clockminute++) {
      for (clockseconds = 0; clockseconds < 60; clockseconds++) {
        Serial.print(clockhour);
        Serial.print(" ");
        Serial.println(clockminute);
        Serial.print(" ");
        Serial.println(clockseconds);

        empty_lights_array();
        strip.clear();
        pixelcolor = strip.Color(0, 0, 0);
        for (int i = 0; i < strip.numPixels(); i++) { // For each pixel in strip...
          strip.setPixelColor(i, pixelcolor);        //  Set pixel's color (in RAM)
        }
        strip.show();

        fill_lights_array();
        strip.clear();
        for (int i = 0; i < strip.numPixels(); i++) { // For each pixel in strip...
          if (lights[i] == 1) {
            strip.setPixelColor(i, clockcolor);        //  Set pixel's color (in RAM)
          }
        }
        strip.show();                          //  Update strip to match
        delay(100);                           //  Pause for a moment
      }
    }

  }
}



// Rainbow cycle along whole strip. Pass delay time (in ms) between frames.
void rainbow(int wait) {
  // Hue of first pixel runs 5 complete loops through the color wheel.
  // Color wheel has a range of 65536 but it's OK if we roll over, so
  // just count from 0 to 5*65536. Adding 256 to firstPixelHue each time
  // means we'll make 5*65536/256 = 1280 passes through this outer loop:
  for (long firstPixelHue = 0; firstPixelHue < 5 * 65536; firstPixelHue += 256) {
    for (int i = 0; i < strip.numPixels(); i++) { // For each pixel in strip...
      // Offset pixel hue by an amount to make one full revolution of the
      // color wheel (range of 65536) along the length of the strip
      // (strip.numPixels() steps):
      int pixelHue = firstPixelHue + (i * 65536L / strip.numPixels());
      // strip.ColorHSV() can take 1 or 3 arguments: a hue (0 to 65535) or
      // optionally add saturation and value (brightness) (each 0 to 255).
      // Here we're using just the single-argument hue variant. The result
      // is passed through strip.gamma32() to provide 'truer' colors
      // before assigning to each pixel:
      strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));
    }
    strip.show(); // Update strip with new contents
    delay(wait);  // Pause for a moment
  }
}

// Rainbow-enhanced theater marquee. Pass delay time (in ms) between frames.
void theaterChaseRainbow(int wait) {
  int firstPixelHue = 0;     // First pixel starts at red (hue 0)
  for (int a = 0; a < 30; a++) { // Repeat 30 times...
    for (int b = 0; b < 3; b++) { //  'b' counts from 0 to 2...
      strip.clear();         //   Set all pixels in RAM to 0 (off)
      // 'c' counts up from 'b' to end of strip in increments of 3...
      for (int c = b; c < strip.numPixels(); c += 3) {
        // hue of pixel 'c' is offset by an amount to make one full
        // revolution of the color wheel (range 65536) along the length
        // of the strip (strip.numPixels() steps):
        int      hue   = firstPixelHue + c * 65536L / strip.numPixels();
        uint32_t color = strip.gamma32(strip.ColorHSV(hue)); // hue -> RGB
        strip.setPixelColor(c, color); // Set pixel 'c' to value 'color'
      }
      strip.show();                // Update strip with new contents
      delay(wait);                 // Pause for a moment
      firstPixelHue += 65536 / 90; // One cycle of color wheel over 90 frames
    }
  }
}



void colorWipe(uint32_t color, int wait) {
  for (int i = 0; i < strip.numPixels(); i++) { // For each pixel in strip...
    strip.setPixelColor(i, color);         //  Set pixel's color (in RAM)
    strip.show();                          //  Update strip to match
    delay(wait);                           //  Pause for a moment
  }
}

void textanim() {
  int  x = matrix.width();
  char yourText[64] = "5555555";
  int  pixelPerChar = 6;
  int  maxDisplacement;


  maxDisplacement = strlen(yourText) * pixelPerChar + matrix.width();

  clockhour = myTZ.hour();
  clockminute = myTZ.minute();
  clockseconds = myTZ.second();



  String myHr;
  myHr = String(clockhour);
  String myMin;
  myMin = String(clockminute);
  if (myMin.length() == 1) {
    myMin = "0" + String(clockminute);
  }
  String myTime;
  myTime = myHr + ":" + myMin + "     ";

  for (int pass = 0; pass < 60; pass++) {

    int i = 0;
    i++;
    //matrix.fillScreen(0);
    for (int y = 0; y < 16; y++)
      for (int x = 0; x < 16; x++)
        if (y & 1)
          matrix.setPixelColor(15 - x + y * 16, matrix.gamma32(matrix.ColorHSV((x + y) * 256 * 8 + i * 128 * 2)));
        else
          matrix.setPixelColor(x + y * 16, matrix.gamma32(matrix.ColorHSV((x + y) * 256 * 8 + i * 128 * 2)));

    matrix.setTextColor(matrix.Color(0, 0, 0));
    //matrix.setCursor(-((millis() / 30) & 127) + 20, 4);
    matrix.setCursor(x, 4);
    matrix.print(myTime);
    if (--x < -maxDisplacement) {
      x = matrix.width();
    }

    matrix.show();
    delay(40);
  }

}

void textanim2() {
  int  x = matrix.width();
  char yourText[64] = "Tick Tock";
  int  pixelPerChar = 6;
  int  maxDisplacement;


  maxDisplacement = strlen(yourText) * pixelPerChar + matrix.width();


  for (int pass = 0; pass < 150; pass++) {

    int i = 0;
    i++;
    //matrix.fillScreen(0);
    for (int y = 0; y < 16; y++)
      for (int x = 0; x < 16; x++)
        if (y & 1)
          matrix.setPixelColor(15 - x + y * 16, matrix.gamma32(matrix.ColorHSV((x + y) * 256 * 8 + i * 128 * 2)));
        else
          matrix.setPixelColor(x + y * 16, matrix.gamma32(matrix.ColorHSV((x + y) * 256 * 8 + i * 128 * 2)));

    matrix.setTextColor(matrix.Color(0, 0, 0));
    matrix.setCursor(-((millis() / 30) & 127) + 20, 4);
    matrix.print(F("Tick Tock"));
    matrix.show();

  }

}

void textanim3() {
  int  x = matrix.width();
  char yourText[64] = "Word Clock";
  int  pixelPerChar = 6;
  int  maxDisplacement;


  maxDisplacement = strlen(yourText) * pixelPerChar + matrix.width();

  for (int pass = 0; pass < 80; pass++) {

    int i = 0;
    i++;
    //matrix.fillScreen(0);
    for (int y = 0; y < 16; y++) {
      for (int x = 0; x < 16; x++) {
        if (y & 1) {
          matrix.setPixelColor(15 - x + y * 16, matrix.gamma32(matrix.ColorHSV((x + y) * 256 * 8 + i * 128 * 2)));
        } else {
          matrix.setPixelColor(x + y * 16, matrix.gamma32(matrix.ColorHSV((x + y) * 256 * 8 + i * 128 * 2)));
        }
      }
    }
    matrix.setTextColor(matrix.Color(0, 0, 0));
    //matrix.setCursor(-((millis() / 30) & 127) + 20, 4);
    matrix.setCursor(x, 4);
    matrix.print(F("Word Clock"));
    if (--x < -maxDisplacement) {
      x = matrix.width();
    }
    matrix.show();
    delay(40);
  }

}

void textanim4() {
  int  x = matrix.width();
  char yourText[64] = "Setting Saved";
  int  pixelPerChar = 6;
  int  maxDisplacement;


  maxDisplacement = strlen(yourText) * pixelPerChar + matrix.width();

  for (int pass = 0; pass < 100; pass++) {

    int i = 0;
    i++;
    //matrix.fillScreen(0);
    for (int y = 0; y < 16; y++) {
      for (int x = 0; x < 16; x++) {
        if (y & 1) {
          matrix.setPixelColor(15 - x + y * 16, matrix.gamma32(matrix.ColorHSV((x + y) * 256 * 8 + i * 128 * 2)));
        } else {
          matrix.setPixelColor(x + y * 16, matrix.gamma32(matrix.ColorHSV((x + y) * 256 * 8 + i * 128 * 2)));
        }
      }
    }
    matrix.setTextColor(matrix.Color(0, 0, 0));
    //matrix.setCursor(-((millis() / 30) & 127) + 20, 4);
    matrix.setCursor(x, 4);
    matrix.print(F("Setting Saved"));
    if (--x < -maxDisplacement) {
      x = matrix.width();
    }
    matrix.show();
    delay(40);
  }

}

void textanim5() {
  int  x = matrix.width();
  char yourText[64] = "Settings";
  int  pixelPerChar = 6;
  int  maxDisplacement;


  maxDisplacement = strlen(yourText) * pixelPerChar + matrix.width();

  for (int pass = 0; pass < 70; pass++) {

    int i = 0;
    i++;
    //matrix.fillScreen(0);
    for (int y = 0; y < 16; y++) {
      for (int x = 0; x < 16; x++) {
        if (y & 1) {
          matrix.setPixelColor(15 - x + y * 16, matrix.gamma32(matrix.ColorHSV((x + y) * 256 * 8 + i * 128 * 2)));
        } else {
          matrix.setPixelColor(x + y * 16, matrix.gamma32(matrix.ColorHSV((x + y) * 256 * 8 + i * 128 * 2)));
        }
      }
    }
    matrix.setTextColor(matrix.Color(0, 0, 0));
    //matrix.setCursor(-((millis() / 30) & 127) + 20, 4);
    matrix.setCursor(x, 4);
    matrix.print(F("Settings"));
    if (--x < -maxDisplacement) {
      x = matrix.width();
    }
    matrix.show();
    delay(40);
  }

}



/*
  The wifi manager is based on https://github.com/tzapu/WiFiManager


  The MIT License (MIT)

  Copyright (c) 2015 tzapu

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.


  and also code from
  https://gitlab.com/MrDIYca/code-samples/-/raw/master/esp8266_setup_portal.ino

*/