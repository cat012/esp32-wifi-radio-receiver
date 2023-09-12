// fox ^^ WiFi Radio Receiver alpha
// 12 Sep 2023


#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiAP.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <Audio.h>
#include <ESP32Ping.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>


Audio audio(true, I2S_DAC_CHANNEL_BOTH_EN);
WebServer server(80);
WiFiMulti wifiMulti;

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET     -1
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

String current_station = "";
uint8_t curstation=0;
uint8_t audiovol=0;
bool sinternet=false;

/* user settigs */
const char *esp_softap_ssid = "esp32fox";
const char *esp_softap_pass = "11223344Lap";

#define WIFI_CONN_TIME_SEC  8

#define WIFI_ADDR_PASS_SIZE   32
#define MAX_STREAM_ADDR_SIZE  64

#define STATIONS_LIST_SIZE  16
/* end of user settigs */

#define STREAMS_EE_SIZE  (STATIONS_LIST_SIZE * MAX_STREAM_ADDR_SIZE)
#define SETTIGS_EE_SIZE  2

#define CURR_STATION_EE_ADDR  0
#define AUDIO_VOLUME_EE_ADDR  1
#define NEW_SSID_EE_ADDR      2
#define NEW_PASS_EE_ADDR      (NEW_SSID_EE_ADDR + WIFI_ADDR_PASS_SIZE)
#define STREAM_0_EE_ADDR      (NEW_PASS_EE_ADDR + WIFI_ADDR_PASS_SIZE) 

#define EEPROM_SIZE  (WIFI_ADDR_PASS_SIZE + WIFI_ADDR_PASS_SIZE + STREAMS_EE_SIZE + SETTIGS_EE_SIZE)

#define MAX_AUDIO_VOL  21


//-----------------------------------------------------------------------------
void handle_root(void)
    {
    String page="";
    page +="<body bgcolor=\"GhostWhite\">";
    page +="<p><a href=\"/\">WiFi Radio Receiver alpha</a></p>";
    
    page +="Station: ";
    page +=curstation;
    page +="<form action=\"/changestation\" method=\"POST\">";
    page +="<input type=\"text\" name=\"nextstation\" placeholder=\"0-";
    page +=STATIONS_LIST_SIZE-1;
    page +="\"></br>";
    page +="<input type=\"submit\" value=\"Save\"></form>";
    page +="Address: <font color=\"DimGray\">";
    page +=current_station;
    page +="</font><form action=\"/replacestation\" method=\"POST\">";
    page +="<input type=\"text\" name=\"newstation\" placeholder=\"New Address\"></br>";
    page +="<input type=\"submit\" value=\"Save\"></form>";
    page +="<p><a href=\"/stations\">Saved Stations</a></p>";
    page +="Volume: ";
    page +=audiovol;
    page +="<form action=\"/changevolume\" method=\"POST\">";
    page +="<input type=\"text\" name=\"newvolume\" placeholder=\"0-21\"></br>";
    page +="<input type=\"submit\" value=\"Save\"></form>";
    page +="SSID: ";
    page +=WiFi.SSID();
    page +="<form action=\"/changeap\" method=\"POST\">";
    page +="<input type=\"text\" name=\"newssid\" placeholder=\"New SSID\"></br>"; 
    page +="<input type=\"text\" name=\"newpass\" placeholder=\"New Password\"></br>";
    page +="<input type=\"submit\" value=\"Save\"></form>";
    if(sinternet==false) page +="<p><font color=\"maroon\">No internet connection</font></p>";
    else page +="<p><font color=\"teal\">Connected to internet</font></p>";
    page +="</body>";
    
    server.send(200, "text/html", page);
    }


//-----------------------------------------------------------------------------
void handle_404(void)
    {
    server.send(404, "text/html", "<p>404: Not found</p><p><a href=\"/\">Home</a></p>");
    }


//-----------------------------------------------------------------------------
void server_send_invalid_request(void)
    { 
    server.send(400, "text/html", "<p>400: Invalid Request</p><p><a href=\"/\">Home</a></p>");
    }


//--------------------------------------------------------------------------------------------
void handle_changestation(void)
    {                    
    if(!server.hasArg("nextstation") || server.arg("nextstation")==NULL) { server_send_invalid_request(); return; }

    String str1=server.arg("nextstation");
    char strbuff[4]="";
    str1.toCharArray(strbuff,4);
    uint8_t result=atoi(strbuff);

    if(result < STATIONS_LIST_SIZE) { curstation = result; }
    else { server_send_invalid_request(); return; }

    if(EEPROM.readUChar(CURR_STATION_EE_ADDR) != curstation)  { EEPROM.writeUChar(CURR_STATION_EE_ADDR, curstation); EEPROM.commit(); }

    current_station = EEPROM.readString(STREAM_0_EE_ADDR+(curstation*MAX_STREAM_ADDR_SIZE));

    audio.stopSong();
    audio.connecttohost((char*)current_station.c_str());

    handle_root();
    }


//--------------------------------------------------------------------------------------------
void handle_replacestation(void)
    {                    
    if(!server.hasArg("newstation") || server.arg("newstation")==NULL) { server_send_invalid_request(); return; }

    String str = server.arg("newstation");
    char new_addr[MAX_STREAM_ADDR_SIZE] = "";
    
    current_station = str;

    str.toCharArray(new_addr, MAX_STREAM_ADDR_SIZE);

    EEPROM.writeString((STREAM_0_EE_ADDR+(curstation*MAX_STREAM_ADDR_SIZE)), new_addr);
    EEPROM.commit();

    audio.stopSong();
    audio.connecttohost((char*)current_station.c_str());
    
    handle_root();
    }


//--------------------------------------------------------------------------------------------
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
    
    handle_root();
    }


//--------------------------------------------------------------------------------------------
void handle_changeap(void)
    {                    
    if(!server.hasArg("newssid") || server.arg("newssid")==NULL || (!server.hasArg("newpass") || server.arg("newpass")==NULL)) { server_send_invalid_request(); return; }

    String str1=server.arg("newssid");
    String str2=server.arg("newpass");

    char wifi_ssid[WIFI_ADDR_PASS_SIZE] = "";
    char wifi_pass[WIFI_ADDR_PASS_SIZE] = "";

    str1.toCharArray(wifi_ssid, WIFI_ADDR_PASS_SIZE);
    str2.toCharArray(wifi_pass, WIFI_ADDR_PASS_SIZE);

    EEPROM.writeString(NEW_SSID_EE_ADDR, wifi_ssid);
    EEPROM.writeString(NEW_PASS_EE_ADDR, wifi_pass);
    EEPROM.commit();

    String page="";
    page +="<p>Saved</p>";
    server.send(200, "text/html", page);

    audio.stopSong();

    delay(1000);  //xxx

    wifi_audio_init((char*)EEPROM.readString(NEW_SSID_EE_ADDR).c_str(), (char*)EEPROM.readString(NEW_PASS_EE_ADDR).c_str());
    }


//--------------------------------------------------------------------------------------------
void handle_show_stations(void)
    {                    
    String page="";
    page +="<body bgcolor=\"GhostWhite\">";
    page +="<p><a href=\"/\">Home</a></p>";
    for(uint8_t s=0; s<STATIONS_LIST_SIZE; s++)
        {
        page += s;
        page += ". <font color=\"DimGray\">";
        page += EEPROM.readString(STREAM_0_EE_ADDR+(s*MAX_STREAM_ADDR_SIZE));
        page += "</font></br>";
        }
    page +="<p><a href=\"/\">Home</a></p>";
    page +="</body>";
    server.send(200, "text/html", page);
    }


//--------------------------------------------------------------------------------------------
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
        audio.setVolume(audiovol); // 0...21

        if(EEPROM.readUChar(CURR_STATION_EE_ADDR) >= STATIONS_LIST_SIZE)  { EEPROM.writeUChar(CURR_STATION_EE_ADDR, 0); EEPROM.commit(); }

        if(Ping.ping("www.google.com", 1)==true || Ping.ping("www.ya.ru", 1)==true)
            {
            curstation=EEPROM.readUChar(CURR_STATION_EE_ADDR);
            current_station = EEPROM.readString(STREAM_0_EE_ADDR+(curstation*MAX_STREAM_ADDR_SIZE));
            audio.connecttohost((char*)current_station.c_str());
            sinternet=true;
            }
        else sinternet=false;
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


//############################################################################################################
void setup(void)
    {
    Serial.begin(115200);
    Serial.println("");
    Serial.println("^^ webradio start program");

    display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
    display.clearDisplay();
    //display.cp437(true);
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE); 
    display.setCursor(0, 0);
    display.println("^^");
    display.display();

    EEPROM.begin(EEPROM_SIZE);

    wifi_audio_init((char*)EEPROM.readString(NEW_SSID_EE_ADDR).c_str(), (char*)EEPROM.readString(NEW_PASS_EE_ADDR).c_str());

    server.on("/", handle_root);
    server.on("/changestation", handle_changestation);
    server.on("/replacestation", handle_replacestation);
    server.on("/changevolume", handle_changevolume);
    server.on("/changeap", handle_changeap);
    server.on("/stations", handle_show_stations);
    server.onNotFound(handle_404);
    server.begin();
    }


//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void loop(void)
    {
    audio.loop();
    server.handleClient();
    }


//--------------------------------------------------------------------------------------------  
void write_streamTitle(String sTitle)
    {
    display.clearDisplay();
    display.setCursor(0, 0);
    if(sTitle=="") display.println("*");
    else display.println(sTitle);
    display.display();
    }


//................................................................................
void audio_info(const char *info)
    {
    Serial.print("audio info: ");
    Serial.println(info);
    }


//................................................................................
void audio_showstreamtitle(const char *info)
    {
    String sinfo=String(info);
    sinfo.replace("|", "\n");
    write_streamTitle(sinfo);
    }



//end of file
