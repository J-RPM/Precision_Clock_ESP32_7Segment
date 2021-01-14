/*
        A part for this code was created from the information of D L Bird 2019
                          See more at http://dsbird.org.uk
           https://www.youtube.com/watch?v=RTKQMuWPL5A&ab_channel=G6EJD-David

          Some routines were created from the information of Andreas Spiess
                        https://www.sensorsiot.org/
                  https://www.youtube.com/c/AndreasSpiess/

           Library: LedController.hpp for MAX7219 LED 7-segment Serial
            https://www.arduinolibraries.info/libraries/led-controller
                      Noa Sakurajin (noasakurajin@web.de)

                                 --- oOo ---
                                
                              Modified by: J_RPM
                               http://j-rpm.com/
                        https://www.youtube.com/c/JRPM
                              January of 2021

              An optional OLED display is added to show the Time an Date,
                  adding some new routines and modifying others.

                              >>> HARDWARE <<<
                  LIVE D1 mini ESP32 ESP-32 WiFi + Bluetooth
                https://es.aliexpress.com/item/32816065152.html
                
            HW-699 0.66 inch OLED display module, for D1 Mini (64x48)
               https://es.aliexpress.com/item/4000504485892.html

                    MAX7219 8-digit LED 7-segment Serial
               https://es.aliexpress.com/item/32956123955.html

                           >>> IDE Arduino <<<
                        Model: WEMOS MINI D1 ESP32
       Add URLs: https://dl.espressif.com/dl/package_esp32_index.json
                     https://github.com/espressif/arduino-esp32

 ____________________________________________________________________________________
*/
String HWversion = "v1.42"; 
#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <time.h>
#include "LedController.hpp"

#include <DNSServer.h>
#include <WiFiManager.h>  //https://github.com/tzapu/WiFiManager
#include <Adafruit_GFX.h>
//*************************************************IMPORTANT******************************************************************
#include "Adafruit_SSD1306.h" // Copy the supplied version of Adafruit_SSD1306.cpp and Adafruit_ssd1306.h to the sketch folder
#define  OLED_RESET 0         // GPIO0
Adafruit_SSD1306 display(OLED_RESET);

String CurrentTime, CurrentDate, nDay, webpage = "";
bool display_EU = true;
int matrix_speed = 25;

// Turn on debug statements to the serial output
#define DEBUG  1
#if  DEBUG
#define PRINT(s, x) { Serial.print(F(s)); Serial.print(x); }
#define PRINTS(x) Serial.print(F(x))
#define PRINTX(x) Serial.println(x, HEX)
#else
#define PRINT(s, x)
#define PRINTS(x)
#define PRINTX(x)
#endif

// ESP32 -> Matrix 
#define DIN_PIN 32 
#define CLK_PIN 27 
#define CS_PIN  25 

// Define the number of bytes you want to access (first is index 0)
#define EEPROM_SIZE 7

////////////////////////// MATRIX //////////////////////////////////////////////
//#define MAX_DIGITS 20
bool _scroll = false;
bool display_date = true;
bool animated_time = true;
bool show_seconds = false;
long clkTime = 0;
int brightness = 5;  //DUTY CYCLE: 11/32
String mDay;
long timeConnect;
String mText;
//////////////////////////////////////////////////////////////////////////////

WiFiClient client;
const char* Timezone    = "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00";  // Choose your time zone from: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv 
                                                           // See below for examples
const char* ntpServer   = "es.pool.ntp.org";               // Or, choose a time server close to you, but in most cases it's best to use pool.ntp.org to find an NTP server
                                                           // then the NTP system decides e.g. 0.pool.ntp.org, 1.pool.ntp.org as the NTP syem tries to find  the closest available servers
                                                           // EU "0.europe.pool.ntp.org"
                                                           // US "0.north-america.pool.ntp.org"
                                                           // See: https://www.ntppool.org/en/                                                           
int  gmtOffset_sec      = 0;    // UK normal time is GMT, so GMT Offset is 0, for US (-5Hrs) is typically -18000, AU is typically (+8hrs) 28800
int  daylightOffset_sec = 7200; // In the UK DST is +1hr or 3600-secs, other countries may use 2hrs 7200 or 30-mins 1800 or 5.5hrs 19800 Ahead of GMT use + offset behind - offset

WebServer server(80); 

/*
 pin 32 is connected to the DIN 
 pin 27 is connected to the CLK 
 pin 25 is connected to CS 
 We have only a single MAX72XX.
 */
LedController<1,1> lc;

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void setup() {
  
  Serial.begin(115200);
  timeConnect = millis();

  lc=LedController<1,1>(DIN_PIN,CLK_PIN,CS_PIN);

  /*
   The MAX72XX is in power-saving mode on startup,
   we have to do a wakeup call
   */
  lc.activateAllSegments();
  /* Set the brightness to a medium values */
  lc.setIntensity(brightness);
  /* and clear the display */
  lc.clearMatrix();
 
  // Test Display LED
  testBCD();
 
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 64x48)
  display.setTextColor(WHITE);
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(15,0);   
  display.println(F("NTP"));
  display.setCursor(6,16);   
  display.println(F("TIME"));

  display.setTextSize(1);   
  display.setCursor(15,32);   
  display.println(HWversion);
  display.setCursor(10,40);   
  display.println(F("Sync..."));
  display.display();

  mText="        NTP TIME  " + HWversion + "  " ;
  scrollText();
  delay(2000);
  
  //------------------------------
  //WiFiManager intialisation. Once completed there is no need to repeat the process on the current board
  WiFiManager wifiManager;
  display_AP_wifi();

  // A new OOB ESP32 will have no credentials, so will connect and not need this to be uncommented and compiled in, a used one will, try it to see how it works
  // Uncomment the next line for a new device or one that has not connected to your Wi-Fi before or you want to reset the Wi-Fi connection
  // wifiManager.resetSettings();
  // Then restart the ESP32 and connect your PC to the wireless access point called 'ESP32_AP' or whatever you call it below
  // Next connect to http://192.168.4.1/ and follow instructions to make the WiFi connection

  // Set a timeout until configuration is turned off, useful to retry or go to sleep in n-seconds
  wifiManager.setTimeout(180);
  
  //fetches ssid and password and tries to connect, if connections succeeds it starts an access point with the name called "ESP32_AP" and waits in a blocking loop for configuration
  if (!wifiManager.autoConnect("ESP32_AP")) {
    PRINTS("\nFailed to connect and timeout occurred");
    display_AP_wifi();
    display_flash();
    reset_ESP32();
  }
  // At this stage the WiFi manager will have successfully connected to a network,
  // or if not will try again in 180-seconds
  //---------------------------------------------------------
  // 
  PRINT("\n>>> Connection Delay(ms): ",millis()-timeConnect);
  if(millis()-timeConnect > 30000) {
    PRINTS("\nTimeOut connection, restarting!!!");
    reset_ESP32();
  }
  
  // Print the IP address
  PRINT("\nUse this URL to connect -> http://",WiFi.localIP());
  PRINTS("/");
  display_ip();
  SetupTime();
  UpdateLocalTime();
  
  // Initialize EEPROM with predefined size
  EEPROM.begin(EEPROM_SIZE);

  // 0 - Display Status 
  display_EU = EEPROM.read(0);
  PRINT("\ndisplay_EU: ",display_EU);

  // 1 - Display Date  
  display_date = EEPROM.read(1);
  PRINT("\ndisplay_date: ",display_date);
  
  // 2 - Matrix Brightness  
  brightness = EEPROM.read(2);
  PRINT("\nbrightness: ",brightness);
  //sendCmdAll(CMD_INTENSITY,brightness);
  lc.setIntensity(brightness);
  
  // 3 - Animated Time  
  animated_time = EEPROM.read(3);
  PRINT("\nanimated_time: ",animated_time);

  // 4 - Show Seconds  
  show_seconds = EEPROM.read(4);
  PRINT("\nshow_seconds: ",show_seconds);

  // 5 - Speed Matrix (delay)  
  matrix_speed = EEPROM.read(5);
  if (matrix_speed < 10 || matrix_speed > 40) {
    matrix_speed = 25;
    EEPROM.write(5, matrix_speed);
  }
  PRINT("\nmatrix_speed: ",matrix_speed);

   // Close EEPROM    
  EEPROM.commit();
  EEPROM.end();

  checkServer();

  // Debug first message 
  String stringMsg = "ESP32_Time_8BCD_JR " + HWversion + " - IP: " + WiFi.localIP().toString();
  PRINT("\nstringMsg >>> ", stringMsg + "\n");

}
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void loop() {
  // Wait for a client to connect and when they do process their requests
  server.handleClient();

  // Update and refresh of the date and time on the displays
  if (millis() % 60000) UpdateLocalTime();
  Oled_Time();
  server.handleClient();
  matrix_time();

  // Show date on matrix display
  if (display_date == true) {
    if(millis()-clkTime > 30000) { // clock for 30s, then scrolls for about 5s
      _scroll=true;
      Oled_Time();
      mText="        " + mDay + "        ";
      scrollText();
      clkTime = millis();
      _scroll=false;
    }
  }
}
//////////////////////////////////////////////////////////////////////////////
boolean SetupTime() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer, "time.nist.gov");  // configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  setenv("TZ", Timezone, 1);                                                  // setenv()adds the "TZ" variable to the environment with a value TimeZone, only used if set to 1, 0 means no change
  tzset();                                                                    // Set the TZ environment variable
  delay(2000);
  bool TimeStatus = UpdateLocalTime();
  return TimeStatus;
}
//////////////////////////////////////////////////////////////////////////////
boolean UpdateLocalTime() {
  struct tm timeinfo;
  time_t now;
  time(&now);

  //See http://www.cplusplus.com/reference/ctime/strftime/
  char output[30];
  strftime(output, 30, "%A", localtime(&now));
  nDay = output;
  mDay = nDay;
  nDay = (nDay.substring(0,3)) + ". ";
  if (display_EU == true) {
    strftime(output, 30,"%d-%m", localtime(&now));
    CurrentDate = nDay + output;
    strftime(output, 30,", %d %B %Y", localtime(&now));
    mDay = mDay + output;
    strftime(output, 30, "%H:%M:%S", localtime(&now));
    CurrentTime = output;
  }
  else { 
    strftime(output, 30, "%m-%d", localtime(&now));
    CurrentDate = nDay + output;
    strftime(output, 30, ", %B %d, %Y", localtime(&now));
    mDay = mDay + output;
    strftime(output, 30, "%r", localtime(&now));
    CurrentTime = output;
  }
  return true;
}
/////////////////////////////////////////////////////
//------------------ OLED DISPLAY -----------------//
/////////////////////////////////////////////////////
void Oled_Time() { 
  display.clearDisplay();
  display.setCursor(2,0);   // center date display
  display.setTextSize(1);   
  display.println(CurrentDate);

  display.setTextSize(2);   
  display.setCursor(8,16);  // center Time display
  if (CurrentTime.startsWith("0")){
    display.println(CurrentTime.substring(1,5));
  }else {
    display.setCursor(0,16);
    display.println(CurrentTime.substring(0,5));
  }
  
  if (display_EU == true) {
    display.setCursor(7,33); // center Time display
    if (_scroll) {
      display.print("----");
    }else{
      display.print("(" + CurrentTime.substring(6,8) + ")");
    }
  }else {
    if (_scroll) {
      display.print("----");
    }else{
      display.print("(" + CurrentTime.substring(6,8) + ")");
    }
    display.setTextSize(1);
    display.setCursor(40,33); 
    display.print(CurrentTime.substring(8,11));
  }
  display.display();
}
/////////////////////////////////////////////////////
//------------------ MATRIX DISPLAY ---------------//
/////////////////////////////////////////////////////
void matrix_time() {
  int n;
  int p;
  if (CurrentTime.substring(9,10)!= "") {
    lc.setChar(0,1,' ',false);
    p=0;
  }else {
    lc.setChar(0,7,' ',false);
    p=1;
  }

  n = (CurrentTime.substring(0,1)).toInt();
  if (n!=0) {
    lc.setDigit(0,7-p,n,false);
  }else {
    lc.setChar(0,7,' ',false);
  }
  n = (CurrentTime.substring(1,2)).toInt();
  lc.setDigit(0,6-p,n,true);
  n = (CurrentTime.substring(3,4)).toInt();
  lc.setDigit(0,5-p,n,false);
  n = (CurrentTime.substring(4,5)).toInt();
  lc.setDigit(0,4-p,n,true);
  n = (CurrentTime.substring(6,7)).toInt();
  lc.setDigit(0,3-p,n,false);
  n = (CurrentTime.substring(7,8)).toInt();
  lc.setDigit(0,2-p,n,false);
  
  if (CurrentTime.substring(9,10)== "P") {
    lc.setChar(0,0,'P',false);
  }else if (CurrentTime.substring(9,10)== "A"){
    lc.setChar(0,0,'A',false);
  }else {
    lc.setChar(0,0,' ',false);
  }
}
//////////////////////////////////////////////////////////////////////////////
void scrollText() {
  // Length (with one extra character for the null terminator)
  int mText_len = mText.length()+1; 
  
  // Prepare the character array (the buffer) 
  char char_array[mText_len];
  
  // Copy it over 
  mText.toCharArray(char_array, mText_len);
  
  for(int i=0;i<mText_len-8;i++) {
    lc.setChar(0,7,char_array[i],false);
    lc.setChar(0,6,char_array[i+1],false);
    lc.setChar(0,5,char_array[i+2],false);
    lc.setChar(0,4,char_array[i+3],false);
    lc.setChar(0,3,char_array[i+4],false);
    lc.setChar(0,2,char_array[i+5],false);
    lc.setChar(0,1,char_array[i+6],false);
    lc.setChar(0,0,char_array[i+7],false);
    delay(matrix_speed*10);
  }
}
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
// A short method of adding the same web page header to some text
//////////////////////////////////////////////////////////////////////////////
void append_webpage_header() {
  // webpage is a global variable
  webpage = ""; // A blank string variable to hold the web page
  webpage += "<!DOCTYPE html><html>"; 
  webpage += "<style>html { font-family:tahoma; display:inline-block; margin:0px auto; text-align:center;}";
  webpage += "#mark      {border: 5px solid #316573 ; width: 1120px;} "; 
  webpage += "#header    {background-color:#C3E0E8; width:1100px; padding:10px; color:#13414E; font-size:36px;}";
  webpage += "#section   {background-color:#E6F5F9; width:1100px; padding:10px; color:#0D7693 ; font-size:14px;}";
  webpage += "#footer    {background-color:#C3E0E8; width:1100px; padding:10px; color:#13414E; font-size:24px; clear:both;}";
 
  webpage += ".button {box-shadow: 0px 10px 14px -7px #276873; background:linear-gradient(to bottom, #599bb3 5%, #408c99 100%);";
  webpage += "background-color:#599bb3; border-radius:8px; color:white; padding:13px 32px; display:inline-block; cursor:pointer;";
  webpage += "text-decoration:none;text-shadow:0px 1px 0px #3d768a; font-size:50px; font-weight:bold; margin:2px;}";
  webpage += ".button:hover {background:linear-gradient(to bottom, #408c99 5%, #599bb3 100%); background-color:#408c99;}";
  webpage += ".button:active {position:relative; top:1px;}";
 
  webpage += ".button2 {box-shadow: 0px 10px 14px -7px #8a2a21; background:linear-gradient(to bottom, #f24437 5%, #c62d1f 100%);";
  webpage += "background-color:#f24437; text-shadow:0px 1px 0px #810e05; }";
  webpage += ".button2:hover {background:linear-gradient(to bottom, #c62d1f 5%, #f24437 100%); background-color:#f24437;}";
  
  webpage += ".line {border: 3px solid #666; border-radius: 300px/10px; height:0px; width:80%;}";
  
  webpage += "input[type=\"text\"] {font-size:42px; width:90%; text-align:left;}";
  
  webpage += "input[type=range]{height:61px; -webkit-appearance:none;  margin:10px 0; width:70%;}";
  webpage += "input[type=range]:focus {outline:none;}";
  webpage += "input[type=range]::-webkit-slider-runnable-track {width:70%; height:30px; cursor:pointer; animate:0.2s; box-shadow: 2px 2px 5px #000000; background:#C3E0E8;border-radius:10px; border:1px solid #000000;}";
  webpage += "input[type=range]::-webkit-slider-thumb {box-shadow:3px 3px 6px #000000; border:2px solid #FFFFFF; height:50px; width:50px; border-radius:15px; background:#316573; cursor:pointer; -webkit-appearance:none; margin-top:-11.5px;}";
  webpage += "input[type=range]:focus::-webkit-slider-runnable-track {background: #C3E0E8;}";
  webpage += "</style>";
 
  webpage += "<link rel=\"stylesheet\" href=\"https://cdnjs.cloudflare.com/ajax/libs/animate.css/4.1.1/animate.min.css\"/>";
  webpage += "<html><head><title>ESP32 NTP Clock</title>";
  webpage += "</head>";
  webpage += "<script>";
  webpage += "function SendBright()";
  webpage += "{";
  webpage += "  strLine = \"\";";
  webpage += "  var request = new XMLHttpRequest();";
  webpage += "  strLine = \"BRIGHT=\" + document.getElementById(\"bright_form\").Bright.value;";
  webpage += "  request.open(\"GET\", strLine, false);";
  webpage += "  request.send(null);";
  webpage += "}";
  webpage += "</script>";
  webpage += "<div id=\"mark\">";
  webpage += "<div id=\"header\"><h1 class=\"animate__animated animate__flash\">NTP - Local Time Clock " + HWversion + "</h1>";
}
//////////////////////////////////////////////////////////////////////////////
void button_Home() {
  webpage += "<p><a href=\"\\HOME\"><type=\"button\" class=\"button\">Refresh WEB</button></a></p>";
}
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void NTP_Clock_home_page() {
  append_webpage_header();
  webpage += "<p><h3 class=\"animate__animated animate__fadeInLeft\">Display Mode is ";
  if (display_EU == true) webpage += "EU"; else webpage += "USA";
  webpage += "</p>[";
  webpage += "(hh:mm:ss) ";
  if (display_date == true) webpage += "& Date"; else webpage += " Only Time";
  webpage += " - B:";     
  webpage += brightness;
  webpage += "]</h3></div>";

  webpage += "<div id=\"section\">";
  button_Home();
  webpage += "<p><a href=\"\\DISPLAY_MODE_USA\"><type=\"button\" class=\"button\">USA mode</button></a>";
  webpage += "<a href=\"\\DISPLAY_MODE_EU\"><type=\"button\" class=\"button\">EU mode</button></a></p>";

  webpage += "<p><a href=\"\\DISPLAY_DATE\"><type=\"button\" class=\"button\">Show Date</button></a>";
  webpage += "<a href=\"\\DISPLAY_NO_DATE\"><type=\"button\" class=\"button\">Only Time</button></a></p><br>";
 
  webpage += "<form id=\"bright_form\">";
  webpage += "<a>Brightness<br>MIN(0)<input type=\"range\" name=\"Bright\" min=\"0\" max=\"15\" value=\"";
  webpage += brightness;
  webpage += "\">(15)MAX</a></form>";
  webpage += "<p><a href=\"\"><type=\"button\" onClick=\"SendBright()\" class=\"button\">Send Brightness</button></a></p>";

  webpage += "<br><hr class=\"line\"><br>";
  webpage += "<p><a href=\"\\RESTART\"><type=\"button\" class=\"button\">Restart ESP32</button></a></p>";
  webpage += "<br><p><a href=\"\\RESET_WIFI\"><type=\"button\" class=\"button button2\">Reset WiFi</button></a></p>";
  webpage += "</div>";
  end_webpage();
}
//////////////////////////////////////////////////////////////////////////////
void reset_wifi() {
  append_webpage_header();
  webpage += "<p><h2>New WiFi Connection</h2></p></div>";
  webpage += "<div id=\"section\">";
  webpage += "<p>&#149; Connect WiFi to SSID: <b>ESP32_AP</b></p>";
  webpage += "<p>&#149; Next connect to: <b><a href=http://192.168.4.1/>http://192.168.4.1/</a></b></p>";
  webpage += "<p>&#149; Make the WiFi connection</p>";
  button_Home();
  webpage += "</div>";
  end_webpage();
  delay(1000);
  WiFiManager wifiManager;
  wifiManager.resetSettings();      // RESET WiFi in ESP32
  reset_ESP32();
}
//////////////////////////////////////////////////////////////
void web_reset_ESP32() {
  append_webpage_header();
  webpage += "<p><h2>Restarting ESP32...</h2></p></div>";
  webpage += "<div id=\"section\">";
  button_Home();
  webpage += "</div>";
  end_webpage();
  delay(1000);
  reset_ESP32();
}
//////////////////////////////////////////////////////////////
void end_webpage(){
  webpage += "<div id=\"footer\">Copyright &copy; J_RPM 2021</div></div></html>\r\n";
  server.send(200, "text/html", webpage);
  PRINTS("\n>>> end_webpage() OK! ");
}
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void display_mode_toggle() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.write(0, display_EU);
  end_Eprom();
}
//////////////////////////////////////////////////////////////
void display_date_toggle() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.write(1, display_date);
  end_Eprom();
}
//////////////////////////////////////////////////////////////////////////////
void brightness_matrix() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.write(2, brightness);
  //sendCmdAll(CMD_INTENSITY,brightness);
  lc.setIntensity(brightness);
  end_Eprom();
}
//////////////////////////////////////////////////////////////
void display_time_mode() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.write(3, animated_time);
  end_Eprom();
}
//////////////////////////////////////////////////////////////
void display_time_view() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.write(4, show_seconds);
  end_Eprom();
}
//////////////////////////////////////////////////////////////
void display_matrix_speed() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.write(5, matrix_speed);
  end_Eprom();
}
//////////////////////////////////////////////////////////////
void end_Eprom() {
  EEPROM.commit();
  EEPROM.end();
}
//////////////////////////////////////////////////////////////
void reset_ESP32() {
  //sendCmdAll(CMD_SHUTDOWN,0);
  lc.clearMatrix();
  ESP.restart();
  delay(5000);
}
//////////////////////////////////////////////////////////////
void display_AP_wifi () {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(4,0);   
  display.println(F("ENTRY"));
  display.setCursor(10,16);   
  display.println(F("WiFi"));
  display.setTextSize(1);   
  display.setCursor(8,32);   
  display.println("ESP32_AP");
  display.setCursor(0,40);   
  display.println(F("192168.4.1"));
  display.display();
}
//////////////////////////////////////////////////////////////
void display_flash() {
  for (int i=0; i<8; i++) {
    display.invertDisplay(true);
    display.display();
    delay (150);
    display.invertDisplay(false);
    display.display();
    delay (150);
  }
}
//////////////////////////////////////////////////////////////
void display_ip() {
  // Print the IP address MATRIX
  mText="        IP: " + WiFi.localIP().toString();
  scrollText();

  // Display OLED 
  display.clearDisplay();
  display.setTextSize(2);   
  display.setCursor(4,8);   
  display.print("ENTRY");
  display.setTextSize(1);   
  display.setCursor(0,26);   
  display.print("http://");
  display.print(WiFi.localIP());
  display.println("/");
  display.display();
  display_flash();
}
//////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
const char *err2Str(wl_status_t code){
  switch (code){
  case WL_IDLE_STATUS:    return("IDLE");           break; // WiFi is in process of changing between statuses
  case WL_NO_SSID_AVAIL:  return("NO_SSID_AVAIL");  break; // case configured SSID cannot be reached
  case WL_CONNECTED:      return("CONNECTED");      break; // successful connection is established
  case WL_CONNECT_FAILED: return("PASSWORD_ERROR"); break; // password is incorrect
  case WL_DISCONNECTED:   return("CONNECT_FAILED"); break; // module is not configured in station mode
  default: return("??");
  }
}
////////////////////////////////////////////////
// Check clock config
////////////////////////////////////////////////
void _display_mode_usa() {
  display_EU = false;
  PRINTS("\n-> DISPLAY_MODE_USA");
  display_mode_toggle();
  responseWeb();
}
/////////////////////////////////////////////////////////////////
void _display_mode_eu() {
  display_EU = true;
  PRINTS("\n-> DISPLAY_MODE_EU");
  display_mode_toggle();
  responseWeb();
}
/////////////////////////////////////////////////////////////////
void _display_date() {
  display_date = true;
  PRINTS("\n-> DISPLAY_DATE");
  display_date_toggle();
  responseWeb();
}
/////////////////////////////////////////////////////////////////
void _display_no_date() {
  display_date = false;
  PRINTS("\n-> DISPLAY_NO_DATE");
  display_date_toggle();
  responseWeb();
}
/////////////////////////////////////////////////////////////////
void _bright_0() {
  PRINTS("\n-> BRIGHT=0");
  brightness = 0;     //DUTY CYCLE: 1/32 (MIN) 
  _save_bright();
}
/////////////////////////////////////////////////////////////////
void _bright_1() {
  PRINTS("\n-> BRIGHT=1");
  brightness = 1;    
  _save_bright();
}
/////////////////////////////////////////////////////////////////
void _bright_2() {
  PRINTS("\n-> BRIGHT=2");
  brightness = 2;
  _save_bright();
}
/////////////////////////////////////////////////////////////////
void _bright_3() {
  PRINTS("\n-> BRIGHT=3");
  brightness = 3;
  _save_bright();
}
/////////////////////////////////////////////////////////////////
void _bright_4() {
  PRINTS("\n-> BRIGHT=4");
  brightness = 4;
  _save_bright();
}
/////////////////////////////////////////////////////////////////
void _bright_5() {
  PRINTS("\n-> BRIGHT=5");
  brightness = 5;
  _save_bright();
}
/////////////////////////////////////////////////////////////////
void _bright_6() {
  PRINTS("\n-> BRIGHT=6");
  brightness = 6;
  _save_bright();
}
/////////////////////////////////////////////////////////////////
void _bright_7() {
  PRINTS("\n-> BRIGHT=7");
  brightness = 7;
  _save_bright();
}
/////////////////////////////////////////////////////////////////
void _bright_8() {
  PRINTS("\n-> BRIGHT=8");
  brightness = 8;
  _save_bright();
}
/////////////////////////////////////////////////////////////////
void _bright_9() {
  PRINTS("\n-> BRIGHT=9");
  brightness = 9;
  _save_bright();
}
/////////////////////////////////////////////////////////////////
void _bright_10() {
  PRINTS("\n-> BRIGHT=10");
  brightness = 10;
  _save_bright();
}
/////////////////////////////////////////////////////////////////
void _bright_11() {
  PRINTS("\n-> BRIGHT=11");
  brightness = 11;
  _save_bright();
}
/////////////////////////////////////////////////////////////////
void _bright_12() {
  PRINTS("\n-> BRIGHT=12");
  brightness = 12;
  _save_bright();
}
/////////////////////////////////////////////////////////////////
void _bright_13() {
  PRINTS("\n-> BRIGHT=13");
  brightness = 13;
  _save_bright();
}
/////////////////////////////////////////////////////////////////
void _bright_14() {
  PRINTS("\n-> BRIGHT=14");
  brightness = 14;
  _save_bright();
}
/////////////////////////////////////////////////////////////////
void _bright_15() {
  PRINTS("\n-> BRIGHT=15");
  brightness = 15;     //DUTY CYCLE: 31/32 (MAX)
  _save_bright();
}
/////////////////////////////////////////////////////////////////
void _save_bright(){
  brightness_matrix();
  responseWeb();
}
/////////////////////////////////////////////////////////////////
void _restart() {
  PRINTS("\n-> RESTART");
  web_reset_ESP32();
}
/////////////////////////////////////////////////////////////////
void _reset_wifi() {
  PRINTS("\n-> RESET_WIFI");
  reset_wifi();
}
/////////////////////////////////////////////////////////////////
void _home() {
  PRINTS("\n-> HOME");
  responseWeb();
}
/////////////////////////////////////////////////////////////////
void responseWeb(){
  PRINTS("\nS_RESPONSE");
  NTP_Clock_home_page();
  PRINTS("\n-> SendPage");
}
/////////////////////////////////////////////////////////////////
void checkServer(){
  server.begin();  // Start the WebServer
  PRINTS("\nWebServer started");
  // Define what happens when a client requests attention
  server.on("/", _home);
  server.on("/DISPLAY_MODE_USA", _display_mode_usa); 
  server.on("/DISPLAY_MODE_EU", _display_mode_eu); 
  server.on("/DISPLAY_DATE", _display_date);
  server.on("/DISPLAY_NO_DATE", _display_no_date);
  server.on("/BRIGHT=0", _bright_0); 
  server.on("/BRIGHT=1", _bright_1); 
  server.on("/BRIGHT=2", _bright_2); 
  server.on("/BRIGHT=3", _bright_3); 
  server.on("/BRIGHT=4", _bright_4); 
  server.on("/BRIGHT=5", _bright_5); 
  server.on("/BRIGHT=5", _bright_6); 
  server.on("/BRIGHT=7", _bright_6); 
  server.on("/BRIGHT=8", _bright_8); 
  server.on("/BRIGHT=9", _bright_9); 
  server.on("/BRIGHT=10", _bright_10); 
  server.on("/BRIGHT=11", _bright_11); 
  server.on("/BRIGHT=12", _bright_12); 
  server.on("/BRIGHT=13", _bright_13); 
  server.on("/BRIGHT=14", _bright_14); 
  server.on("/BRIGHT=15", _bright_15); 
  server.on("/HOME", _home);
  server.on("/RESTART", _restart);  
  server.on("/RESET_WIFI", _reset_wifi);
}
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
void testBCD() {
  for(int i=0;i<8;i++) {
    lc.setDigit(0,i,8,true);
  }
  delay(1000);
  lc.clearMatrix();
}
////////////////////////// END //////////////////////////////////
/////////////////////////////////////////////////////////////////
