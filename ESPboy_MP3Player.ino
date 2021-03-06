/*
ESPboy MP3 Player by RomanS

ESPboy project page:
https://hackaday.io/project/164830-espboy-beyond-the-games-platform-with-wifi
*/

#include <TFT_eSPI.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_MCP23017.h>
#include <Adafruit_MCP4725.h>
#include <ESP8266WiFi.h>
#include <pgmspace.h>
#include "ESPboyLogo.h"
#include <DFMiniMp3.h>
#include <SoftwareSerial.h>


#define LEDpin            D4
#define SOUNDpin          D3
#define TXpin             1
#define RXpin             D6
#define MCP23017address       0 // actually it's 0x20 but in <Adafruit_MCP23017.h> lib there is (x|0x20) :)
#define csTFTMCP23017pin      8
#define LEDquantity           1
#define MCP4725address        0   

#define MP3VOLUME             20
#define MP3EQ                 DfMp3_Eq_Normal
#define MP3OUTPUT             DfMp3_PlaySource_Sd


//buttons
#define LEFT_BUTTON   (buttonspressed & 1)
#define UP_BUTTON     (buttonspressed & 2)
#define DOWN_BUTTON   (buttonspressed & 4)
#define RIGHT_BUTTON  (buttonspressed & 8)
#define ACT_BUTTON    (buttonspressed & 16)
#define ESC_BUTTON    (buttonspressed & 32)
#define LFT_BUTTON    (buttonspressed & 64)
#define RGT_BUTTON    (buttonspressed & 128)


//diff vars button
static uint_fast16_t  buttonspressed;
static uint_fast16_t count;
static uint16_t mp3vol, mp3state, mp3eq, mp3fileCounts, mp3currentTrack; 
static String consolestrings[12];
static uint16_t consolestringscolor[12];
static unsigned long counttime;
static int16_t lcdbrightnes = 4095;

enum mp3status {STOP = 0, PAUSE = 1, PLAY = 2} mp3onplay = STOP;
enum mp3randstat {OFF = 0, ON = 1} mp3random = OFF;

void drawConsole(String strbfr, uint16_t color);
void screenon();
void readMP3state();


class Mp3Notify{
public:
  static void OnError(uint16_t errorCode){
    drawConsole(F("Com Error"), TFT_RED);
    drawConsole(F("Check SD card"), TFT_MAGENTA);
  }
  static void OnPlayFinished(uint16_t track){
    drawConsole(F("Play finished"), TFT_MAGENTA);
    mp3onplay = STOP;
    screenon();
    readMP3state(); 
  }
  static void OnCardOnline(uint16_t code){
    drawConsole(F("Card online"), TFT_MAGENTA);
  }
  static void OnUsbOnline(uint16_t code){
    drawConsole(F("USB Disk online"), TFT_MAGENTA);
  }
  static void OnCardInserted(uint16_t code){
    drawConsole(F("Card inserted"), TFT_MAGENTA);
    mp3onplay = STOP;
    screenon();
    readMP3state(); 
  }
  static void OnUsbInserted(uint16_t code){
    drawConsole(F("USB Disk inserted"), TFT_MAGENTA);
  }
  static void OnCardRemoved(uint16_t code){
    drawConsole(F("Card removed"), TFT_MAGENTA);
    mp3onplay = STOP;
    screenon();
    readMP3state(); 
  }
  static void OnUsbRemoved(uint16_t code){
    drawConsole(F("USB Disk removed"), TFT_MAGENTA);   
  }
};



//diff vars button
Adafruit_MCP23017 mcp;
Adafruit_NeoPixel pixels(LEDquantity, LEDpin);
Adafruit_MCP4725 dac;
TFT_eSPI tft = TFT_eSPI();
SoftwareSerial secondarySerial(RXpin, TXpin); // RX, TX
DFMiniMp3<SoftwareSerial, Mp3Notify> mp3(secondarySerial);
ADC_MODE(ADC_VCC); //ESP.getVcc();



void readMP3state(){
  mp3state = mp3.getPlaybackMode(); //read mp3 state
  mp3vol = mp3.getVolume(); //read current volume
  mp3eq = mp3.getEq(); //read EQ setting
  mp3fileCounts = mp3.getTotalTrackCount(); //read all file counts in SD card
  mp3currentTrack = mp3.getCurrentTrack(); //read current play file number
}


uint_fast16_t checkbuttons(){
  buttonspressed = ~mcp.readGPIOAB() & 255;
  return (buttonspressed);
}


uint_fast16_t waitkeyunpressed(){
  unsigned long starttime = millis();
  while (checkbuttons())
    delay(5);
  return (millis() - starttime);
}


void drawConsole(String bfrstr, uint16_t color){
  for (int i=0; i<11; i++) {
    consolestrings[i] = consolestrings[i+1];
    consolestringscolor[i] = consolestringscolor[i+1];
  }
  consolestrings[11] = bfrstr;
  consolestringscolor[11] = color;
  tft.fillRect(0,24,128,104,TFT_BLACK);
  for (int i=0; i<12; i++){
    tft.setCursor(4, i*8+28);
    tft.setTextColor(consolestringscolor[i]);
    tft.print(consolestrings[i]);
  }
  tft.drawRect(0, 24, 128, 104, TFT_NAVY);
  batshow();
}



void runButtonsCommand(){
 uint16_t keypressedtime;
  tone (SOUNDpin, 800, 20);
  pixels.setPixelColor(0, pixels.Color(0,0,20));
  pixels.show();  
  secondarySerial.flush();
  Serial.flush();
  delay (50);
  
  if (LEFT_BUTTON && mp3currentTrack > 1) {
    mp3.prevTrack(); 
    readMP3state();
    mp3onplay = PLAY; 
    mp3random = OFF; 
    drawConsole((String)F("play prev ") + (String)mp3currentTrack, TFT_YELLOW);}
  if (RIGHT_BUTTON && mp3currentTrack < mp3fileCounts) { 
    mp3.nextTrack(); 
    readMP3state();
    mp3onplay = PLAY; 
    mp3random = OFF;  
    drawConsole((String)F("play next ") + (String)mp3currentTrack,TFT_YELLOW);}
  if (UP_BUTTON && mp3vol < 30) { 
    mp3.increaseVolume(); 
    readMP3state();
    drawConsole((String)F("volume up ") + (String)mp3vol, TFT_YELLOW);}
  if (DOWN_BUTTON && mp3vol > 0) { 
    mp3.decreaseVolume();
    readMP3state(); 
    drawConsole((String)F("volume down ") + (String)mp3vol, TFT_YELLOW);}
  if (ESC_BUTTON){ 
    keypressedtime = waitkeyunpressed();
    if (keypressedtime < 1000){
      mp3.playRandomTrackFromAll();
      readMP3state();
      mp3random = ON; 
      mp3onplay = PLAY; 
      drawConsole(F("random play"),TFT_YELLOW); 
    }
    else {
      drawConsole((String)F("state: ") + (String)mp3state, TFT_WHITE);
      drawConsole((String)F("volume: ") + (String)mp3vol, TFT_WHITE);
      drawConsole((String)F("EQ: ") + (String)mp3eq, TFT_WHITE);
      drawConsole((String)F("file counts: ") + (String)mp3fileCounts, TFT_WHITE);
      drawConsole((String)F("current track: ") + (String)mp3currentTrack, TFT_WHITE);
    }
  }
  if (ACT_BUTTON) {
    keypressedtime = waitkeyunpressed();
    if (keypressedtime < 1000){
       if (mp3onplay == PLAY){ 
        mp3.pause(); 
        readMP3state();
        mp3onplay = PAUSE; 
        drawConsole(F("pause"),TFT_YELLOW);}
       else { 
        mp3.start(); 
        readMP3state();
        mp3onplay = PLAY; 
        drawConsole(F("continue play"),TFT_YELLOW); }
    }
    else { 
      mp3.stop(); 
      readMP3state();
      mp3onplay = STOP; 
      tone (SOUNDpin, 200, 100);
      drawConsole(F("stop play"),TFT_YELLOW);}
  }
  
  waitkeyunpressed();
  delay(100);
  
  if (mp3onplay == PLAY) 
    if (mp3random == OFF) pixels.setPixelColor(0, pixels.Color(0,20,0));
    else pixels.setPixelColor(0, pixels.Color(10,10,0));
  if (mp3onplay == PAUSE) pixels.setPixelColor(0, pixels.Color(3,0,0));
  if (mp3onplay == STOP) pixels.setPixelColor(0, pixels.Color(0,0,0));
  pixels.show();  
}



void batshow(){
 uint16_t btt[10], sum;
  tft.setTextColor(TFT_MAGENTA);
  tft.setCursor(108, 118);
  for (int i=0; i<10; i++)
     btt[i] = map (ESP.getVcc(), 2600, 3000, 0, 99);
  sum = 0;
  for (int i=0; i<10; i++)
     sum += btt[i];
  sum /= 10;
  if (sum > 99) sum = 99;
  tft.print(sum);
  tft.print("%");
}


void screenon(){
    lcdbrightnes = 4095;
    dac.setVoltage(lcdbrightnes, true);
    runButtonsCommand();
    counttime = millis();
}

void screenoff(){
   lcdbrightnes = 0;
   pixels.setPixelColor(0, pixels.Color(0,0,0));
   pixels.show();
   pixels.show();  
   digitalWrite(D4, HIGH);
}


void setup(){
  for (int i=0; i<8; i++) consolestrings[i] = "";
  
  WiFi.mode(WIFI_OFF); // to safe some battery power
//serial init
  Serial.begin(9600, SERIAL_8N1, SERIAL_RX_ONLY);
  secondarySerial.begin(9600);

//buttons on mcp23017 init
  mcp.begin(MCP23017address);
  delay (100);
  for (int i = 0; i < 8; i++)
  {
    mcp.pinMode(i, INPUT);
    mcp.pullUp(i, HIGH);
  }

//DAC init
  dac.begin(0x60);
  delay(100);
  dac.setVoltage(0, true);


//TFT init
  mcp.pinMode(csTFTMCP23017pin, OUTPUT);
  mcp.digitalWrite(csTFTMCP23017pin, LOW);
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  
//draw ESPboylogo
  tft.drawXBitmap(30, 20, ESPboyLogo, 68, 64, TFT_YELLOW);
  tft.setTextSize(1);
  tft.setTextColor(TFT_YELLOW);
  tft.setCursor(12, 95);
  tft.print(F("MP3 player init..."));

//LED init
  pinMode(LEDpin, OUTPUT);
  pixels.begin();
  delay(100);
  pixels.setPixelColor(0, pixels.Color(0, 0, 0));
  pixels.show();

//sound init and test
  pinMode(SOUNDpin, OUTPUT);
  tone(SOUNDpin, 200, 100);
  delay(100);
  tone(SOUNDpin, 100, 100);
  delay(100);
  noTone(SOUNDpin);

//LCD backlit on
  for (count=0; count<1000; count+=50)
  {
    dac.setVoltage(count, false);
    delay(50);
  }
  dac.setVoltage(4095, true);

//DFplayer init  
  mp3.begin(); 
  mp3.setVolume(MP3VOLUME);
  mp3.decreaseVolume();
  if (MP3VOLUME != (mp3.getVolume()+1))
  {
    tft.fillRect(0, 0, 128, 24, TFT_BLACK);
    tft.setTextSize(1);
    tft.setTextColor(TFT_RED);
    tft.setCursor(2*8, 60);
    tft.print(F("MP3 module fault"));
    while(true) delay(10);
  }
  
  mp3.reset(); 
  mp3.setVolume(MP3VOLUME);  
  mp3.setEq(MP3EQ);
  mp3.setPlaybackSource(MP3OUTPUT);
  mp3.stop();
  readMP3state();

//BAT voltage measure init
  pinMode(A0, INPUT);
  
//clear TFT
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(TFT_MAGENTA);
  tft.setCursor(13, 8);
  tft.print(F("ESPboy MP3 player"));
  drawConsole(F("Up/Dowm - volume"), TFT_WHITE);
  drawConsole(F("Rignt - next"), TFT_WHITE);
  drawConsole(F("Left - previous"), TFT_WHITE);
  drawConsole(F("A - pause/continue"), TFT_WHITE);
  drawConsole(F("long A - stop"), TFT_WHITE);
  drawConsole(F("B - random"), TFT_WHITE);
  drawConsole(F("long B - stat"), TFT_WHITE);

  counttime = millis();
}


void loop(){
  if ((millis()-counttime > 7000) && mp3onplay == PLAY) 
    if (lcdbrightnes > 1){
      lcdbrightnes -= 50;
      if (lcdbrightnes < 0) {
       screenoff();
       lcdbrightnes = 0;
       counttime = millis();
      }
      dac.setVoltage(lcdbrightnes, false);
    }
  if (checkbuttons()) screenon();
  mp3.loop();
  if(!lcdbrightnes) delay(300);
  else delay (100);
}
