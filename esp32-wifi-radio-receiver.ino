// fox ^^ WiFi Radio Receiver alpha
// 14 Sep 2023


#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiAP.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <Audio.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>


/* user settigs */
const char *esp_softap_ssid = "esp32fox";
const char *esp_softap_pass = "11223344Lap";

#define WIFI_CONN_TIME_SEC  8

#define WIFI_SSID_PASS_SIZE   32
#define STREAM_ADDRESS_SIZE  64

#define STATIONS_LIST_SIZE  16
/* end of user settigs */

#define STREAMS_EE_SIZE  (STATIONS_LIST_SIZE * STREAM_ADDRESS_SIZE)
#define SETTIGS_EE_SIZE  2

#define CURR_STATION_EE_ADDR  0
#define AUDIO_VOLUME_EE_ADDR  1
#define NEW_SSID_EE_ADDR      2
#define NEW_PASS_EE_ADDR      (NEW_SSID_EE_ADDR + WIFI_SSID_PASS_SIZE)
#define STREAM_0_EE_ADDR      (NEW_PASS_EE_ADDR + WIFI_SSID_PASS_SIZE) 

#define EEPROM_SIZE  (WIFI_SSID_PASS_SIZE + WIFI_SSID_PASS_SIZE + STREAMS_EE_SIZE + SETTIGS_EE_SIZE)

#define MAX_AUDIO_VOL  21

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET     -1
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

Audio audio(true, I2S_DAC_CHANNEL_BOTH_EN);
WebServer server(80);

String current_station = "";
uint8_t curstation=0;
uint8_t audiovol=0;
bool audiomute=false;


//-------------------------------------------------------------------------------------------------
void handle_root(void)
    {
    String page="";
    page +="<html><head><meta charset=\"utf-8\"><title>WiFi Radio</title></head>";
    page +="<body bgcolor=\"GhostWhite\"><p><a href=\"/\">WiFi Radio Receiver alpha</a></p>";
    page +="<form action=\"/nextstation\"method=\"POST\">";
    page +="<p><input type=\"submit\"name=\"amute\"value=\"Mute\">";
    if(audiomute) page +=" <font color=\"FireBrick\">Muted</font>";
    page +="</p>";
    page +="<input type=\"submit\"name=\"firststa\"value=\"First\"> ";
    page +="<input type=\"submit\"name=\"prevsta\"value=\"Prev\"> ";
    page +=curstation+1;
    page +=" <input type=\"submit\"name=\"nextsta\"value=\"Next\"> ";
    page +="<input type=\"submit\"name=\"laststa\"value=\"Last\"></form>";
    page +="<form action=\"/changestation\" method=\"POST\">";
    page +="<input type=\"text\"name=\"numstation\"size=\"5\"placeholder=\"1-";
    page +=STATIONS_LIST_SIZE;
    page +="\"></br>";
    page +="<input type=\"submit\"value=\"Save\"></form>";
    page +="<font color=\"DimGray\">";
    page +=current_station;
    page +="</font><form action=\"/replacestation\"method=\"POST\">";
    page +="<input type=\"text\"name=\"newstation\"size=\"40\"placeholder=\"New Address\"></br>";
    page +="<input type=\"submit\"value=\"Save\"></form>";
    page +="<p><a href=\"/stations\">Saved Stations</a></p>";
    page +="Volume: ";
    page +=audiovol;
    page +="<form action=\"/changevolume\"method=\"POST\">";
    page +="<input type=\"text\" name=\"newvolume\"size=\"5\"placeholder=\"0-21\"></br>";
    page +="<input type=\"submit\"value=\"Save\"></form>";
    page +="SSID: ";
    page +=WiFi.SSID();
    page +="<form action=\"/changeap\" method=\"POST\">";
    page +="<input type=\"text\"name=\"newssid\"placeholder=\"New SSID\"></br>"; 
    page +="<input type=\"text\"name=\"newpass\"placeholder=\"New Password\"></br>";
    page +="<input type=\"submit\"value=\"Save\"></form>";
    page +="<form action=\"/systemrestart\"method=\"POST\">";
    page +="<input type=\"submit\"name=\"srestart\"value=\"Restart\"></form>";
    page +="</body></html>";

    server.send(200, "text/html", page);
    }


//-------------------------------------------------------------------------------------------------
void handle_404(void)
    {
    server.send(404, "text/html", "<p>404: Not found</p><p><a href=\"/\">Home</a></p>");
    }


//-------------------------------------------------------------------------------------------------
void server_send_invalid_request(void)
    {
    server.send(400, "text/html", "<p>400: Invalid Request</p><p><a href=\"/\">Home</a></p>");
    }


//-------------------------------------------------------------------------------------------------
void handle_navstation(void)
    {
    if(server.hasArg("firststa")) { curstation=0; }
    else if(server.hasArg("nextsta")) { if(++curstation >= STATIONS_LIST_SIZE) curstation=0; }
    else if(server.hasArg("prevsta")) { if(curstation>0) curstation--; else curstation=(STATIONS_LIST_SIZE-1); }
    else if(server.hasArg("laststa")) { curstation=(STATIONS_LIST_SIZE-1); }
    else if(server.hasArg("amute"))
        {
        if(audiomute) { audiomute=false; audiovol=EEPROM.readUChar(AUDIO_VOLUME_EE_ADDR); audio.setVolume(audiovol); }
        else { audiomute=true; audio.setVolume(0); }
        handle_root();
        return;
        }
    else { server_send_invalid_request(); return; }

    if(EEPROM.readUChar(CURR_STATION_EE_ADDR) != curstation)  { EEPROM.writeUChar(CURR_STATION_EE_ADDR, curstation); EEPROM.commit(); }

    current_station = EEPROM.readString(STREAM_0_EE_ADDR+(curstation*STREAM_ADDRESS_SIZE));

    audio.stopSong();
    audio.connecttohost((char*)current_station.c_str());

    handle_root();
    }


//-------------------------------------------------------------------------------------------------
void handle_changestation(void)
    {
    if(!server.hasArg("numstation") || server.arg("numstation")==NULL) { server_send_invalid_request(); return; }

    String str1=server.arg("numstation");
    char strbuff[4]="";
    str1.toCharArray(strbuff,4);
    uint8_t result=atoi(strbuff);

    if(result <= STATIONS_LIST_SIZE && result >0) { curstation = result-1; }
    else { server_send_invalid_request(); return; }

    if(EEPROM.readUChar(CURR_STATION_EE_ADDR) != curstation)  { EEPROM.writeUChar(CURR_STATION_EE_ADDR, curstation); EEPROM.commit(); }

    current_station = EEPROM.readString(STREAM_0_EE_ADDR+(curstation*STREAM_ADDRESS_SIZE));

    audio.stopSong();
    audio.connecttohost((char*)current_station.c_str());

    handle_root();
    }


//-------------------------------------------------------------------------------------------------
void handle_replacestation(void)
    {
    if(!server.hasArg("newstation") || server.arg("newstation")==NULL) { server_send_invalid_request(); return; }

    String str = server.arg("newstation");
    char new_addr[STREAM_ADDRESS_SIZE] = "";
    
    current_station = str;

    str.toCharArray(new_addr, STREAM_ADDRESS_SIZE);

    EEPROM.writeString((STREAM_0_EE_ADDR+(curstation*STREAM_ADDRESS_SIZE)), new_addr);
    EEPROM.commit();

    audio.stopSong();
    audio.connecttohost((char*)current_station.c_str());

    handle_root();
    }


//-------------------------------------------------------------------------------------------------
void handle_show_stations(void)
    {
    String page="";
    page +="<body bgcolor=\"GhostWhite\">";
    page +="<p><a href=\"/\">Home</a></p>";
    for(uint8_t s=0; s<STATIONS_LIST_SIZE; s++)
        {
        page += s+1;
        page += ". <font color=\"DimGray\">";
        page += EEPROM.readString(STREAM_0_EE_ADDR+(s*STREAM_ADDRESS_SIZE));
        page += "</font></br>";
        }
    page +="<p><a href=\"/\">Home</a></p>";
    page +="</body>";
    server.send(200, "text/html", page);
    }


//-------------------------------------------------------------------------------------------------
void handle_changevolume(void)
    {
    if(!server.hasArg("newvolume") || server.arg("newvolume")==NULL) { server_send_invalid_request(); return; }

    String str1=server.arg("newvolume");
    char strbuff[3]="";
    str1.toCharArray(strbuff,3);
    uint8_t result=atoi(strbuff);

    if(result <= MAX_AUDIO_VOL) { audiovol = result; }
    else { server_send_invalid_request(); return; }

    if(EEPROM.readUChar(AUDIO_VOLUME_EE_ADDR) != audiovol)  { EEPROM.writeUChar(AUDIO_VOLUME_EE_ADDR, audiovol); EEPROM.commit(); }
    audio.setVolume(audiovol);
    audiomute=false;

    handle_root();
    }


//-------------------------------------------------------------------------------------------------
void handle_system_restart(void)
    {
    if(!server.hasArg("srestart")) { server_send_invalid_request(); return; }
    String page="";
    page +="<body bgcolor=\"GhostWhite\">";
    page +="Restart";
    page +="<p><a href=\"/\">Home</a></p></body>";
    server.send(200, "text/html", page);
    audio.stopSong();
    delay(1000);
    ESP.restart();
    }


//-------------------------------------------------------------------------------------------------
static void wifi_audio_init(char *ssid, char *pass)
    {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_STA);

    WiFi.begin(ssid, pass);

    for(uint8_t c=0; (WiFi.status() != WL_CONNECTED) && (c<WIFI_CONN_TIME_SEC); c++)
        {
        delay(1000);
        Serial.print(".");
        }

    if(WiFi.status() == WL_CONNECTED)
        {
        Serial.print("^^ webradio local IP: "); Serial.println(WiFi.localIP());

        display.clearDisplay();
        display.setCursor(0, 0);
        display.println(WiFi.localIP());
        display.display();
        delay(1000);

        if(EEPROM.readUChar(AUDIO_VOLUME_EE_ADDR) > MAX_AUDIO_VOL)  { EEPROM.writeUChar(AUDIO_VOLUME_EE_ADDR, MAX_AUDIO_VOL); EEPROM.commit(); }
        audiovol=EEPROM.readUChar(AUDIO_VOLUME_EE_ADDR);
        audio.setVolume(audiovol);

        if(EEPROM.readUChar(CURR_STATION_EE_ADDR) >= STATIONS_LIST_SIZE)  { EEPROM.writeUChar(CURR_STATION_EE_ADDR, 0); EEPROM.commit(); }
        curstation=EEPROM.readUChar(CURR_STATION_EE_ADDR);

        current_station = EEPROM.readString(STREAM_0_EE_ADDR+(curstation*STREAM_ADDRESS_SIZE));
        audio.connecttohost((char*)current_station.c_str());
        }
    else
        {
        WiFi.disconnect(true);
        WiFi.softAP(esp_softap_ssid, esp_softap_pass);
        IPAddress myIP = WiFi.softAPIP();
        Serial.print("^^ webradio AP IP: ");
        Serial.println(myIP);

        display.clearDisplay();
        display.setCursor(0, 0);
        display.println(esp_softap_ssid);
        display.setCursor(0, 10);
        display.println(esp_softap_pass);
        display.setCursor(0, 20);
        display.println(myIP);
        display.display();
        }
    }


//-------------------------------------------------------------------------------------------------
void handle_changeap(void)
    {
    if(!server.hasArg("newssid") || server.arg("newssid")==NULL || (!server.hasArg("newpass") || server.arg("newpass")==NULL)) { server_send_invalid_request(); return; }

    String str1=server.arg("newssid");
    String str2=server.arg("newpass");

    char wifi_ssid[WIFI_SSID_PASS_SIZE] = "";
    char wifi_pass[WIFI_SSID_PASS_SIZE] = "";

    str1.toCharArray(wifi_ssid, WIFI_SSID_PASS_SIZE);
    str2.toCharArray(wifi_pass, WIFI_SSID_PASS_SIZE);

    EEPROM.writeString(NEW_SSID_EE_ADDR, wifi_ssid);
    EEPROM.writeString(NEW_PASS_EE_ADDR, wifi_pass);
    EEPROM.commit();

    String page="";
    page +="<p>Saved</p>";
    server.send(200, "text/html", page);

    audio.stopSong();
    delay(1000);
    wifi_audio_init((char*)EEPROM.readString(NEW_SSID_EE_ADDR).c_str(), (char*)EEPROM.readString(NEW_PASS_EE_ADDR).c_str());
    }


//#################################################################################################
void setup(void)
    {
    Serial.begin(115200);
    Serial.println("");
    Serial.println("^^ webradio start program");

    display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE); 
    display.setCursor(0, 0);
    display.println("^^");
    display.display();

    EEPROM.begin(EEPROM_SIZE);

    wifi_audio_init((char*)EEPROM.readString(NEW_SSID_EE_ADDR).c_str(), (char*)EEPROM.readString(NEW_PASS_EE_ADDR).c_str());

    server.on("/", handle_root);
    server.on("/changestation", handle_changestation);
    server.on("/nextstation", handle_navstation);
    server.on("/replacestation", handle_replacestation);
    server.on("/changevolume", handle_changevolume);
    server.on("/changeap", handle_changeap);
    server.on("/stations", handle_show_stations);
    server.on("/systemrestart", handle_system_restart);
    server.onNotFound(handle_404);
    server.begin();
    }


//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void loop(void)
    {
    audio.loop();
    server.handleClient();
    }


//-------------------------------------------------------------------------------------------------
void write_stream_title(String stitle)
    {
    display.clearDisplay();
    display.setCursor(0, 0);
    if(stitle==String("")) display.println(WiFi.localIP());
    else display.println(stitle);
    display.display();
    }


//.................................................................................................
void audio_info(const char *info)
    {
    Serial.print("audio info: ");
    Serial.println(info);
    }


//..................................................................................................
void audio_showstreamtitle(const char *info)
    {
    String sinfo=String(info);
    sinfo.replace("|", "\n");
    write_stream_title(sinfo);
    }



//end of file
