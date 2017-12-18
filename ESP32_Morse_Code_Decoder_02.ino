/*
 Morse Code Decoder using an OLED and basic microphone

 The MIT License (MIT) Copyright (c) 2017 by David Bird. 
 ### The formulation and calculation method of an IAQ - Internal Air Quality index ###
 ### The provision of a general purpose webserver ###
 Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files 
 (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, 
 publish, distribute, but not to use it commercially for profit making or to sub-license and/or to sell copies of the Software or to 
 permit persons to whom the Software is furnished to do so, subject to the following conditions:  
   The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software. 
   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES 
   OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE 
   LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN 
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. 
 See more at http://dsbird.org.uk 
 
 CW Decoder by Hjalmar Skovholm Hansen OZ1JHM  VER 1.01
 Feel free to change, copy or what ever you like but respect
 that license is http://www.gnu.org/copyleft/gpl.html
 Read more here http://en.wikipedia.org/wiki/Goertzel_algorithm 
 Adapted for the ESP32/ESP8266 by G6EJD  
*/
#include "SH1106.h"     // https://github.com/squix78/esp8266-oled-ssd1306
SH1106 display(0x3c, 17,16); // 1.3" OLED display object definition (address, SDA, SCL) Connect OLED SDA , SCL pins to ESP SDA, SCL pins

float magnitude           = 0;;
int   magnitudelimit      = 100;
int   magnitudelimit_low  = 100;
int   realstate           = LOW;
int   realstatebefore     = LOW;
int   filteredstate       = LOW;
int   filteredstatebefore = LOW;

// The sampling frequency is 45000 on an 80MHz CPU 
// you can set the tuning tone to 496, 558, 744 or 992
// 'n' the number of samples determines bandwidth

float coeff;
float Q1 = 0;
float Q2 = 0;
float sine;
float cosine;
float sampling_freq = 45000;
float target_freq   = 558.0; // adjust for your needs see above
int   n = 128;               // if you change here please change next line also
int   testData[128];
float bw;

// Noise Blanker time which shall be computed so this is initial 
int nbtime = 6;  /// ms noise blanker

long starttimehigh;
long highduration;
long lasthighduration;
long hightimesavg;
long lowtimesavg;
long startttimelow;
long lowduration;
long laststarttime = 0;
#define num_chars 14
char CodeBuffer[num_chars];
char DisplayLine[num_chars+1];
int  stop = LOW;
int  wpm;

void setup() {
  Wire.begin(17,16); // SDA, SCL
  display.init();
  display.setFont(ArialMT_Plain_10);
  display.flipScreenVertically(); // Adjust to suit or remove
  //////////////////////////////////// The basic goertzel calculation //////////////////////////////////////
  target_freq = 496.0; // tune your radio this this beat frequency
  //   target_freq=558.0;
  //   target_freq=744.0;
  //   target_freq=992.0;
  bw = sampling_freq / n;
  int  k;
  float omega;
  k = (int) (0.5 + ((n * target_freq) / sampling_freq));
  omega = (2.0 * PI * k) / n;
  sine = sin(omega);
  cosine = cos(omega);
  coeff = 2.0 * cosine;
  Serial.begin(115200);
  for (int i = 0; i <= num_chars; i++) DisplayLine[i] = ' ';
}

void loop() {
  for (char index = 0; index < n; index++) {testData[index] = analogRead(A0);}
  for (char index = 0; index < n; index++) {
    float Q0;
    Q0 = coeff * Q1 - Q2 + (float) testData[index];
    Q2 = Q1;
    Q1 = Q0;
  }
  float magnitudeSquared = (Q1 * Q1) + (Q2 * Q2) - Q1 * Q2 * coeff; // we do only need the real part //
  magnitude = sqrt(magnitudeSquared);
  Q2 = 0;
  Q1 = 0;

  if (magnitude > magnitudelimit_low) { magnitudelimit = (magnitudelimit + ((magnitude - magnitudelimit) / 6)); } /// moving average filter
  if (magnitudelimit < magnitudelimit_low) magnitudelimit = magnitudelimit_low;

  // Now check the magnitude //
  if (magnitude > magnitudelimit * 0.6) // just to have some space up
    realstate = HIGH;
  else
    realstate = LOW;

  // Clean up the state with a noise blanker //
  
  if (realstate != realstatebefore) {laststarttime = millis();}
  if ((millis() - laststarttime) > nbtime) {
    if (realstate != filteredstate) {
      filteredstate = realstate;
    }
  }

  if (filteredstate != filteredstatebefore) {
    if (filteredstate == HIGH) {
      starttimehigh = millis();
      lowduration = (millis() - startttimelow);
    }

    if (filteredstate == LOW) {
      startttimelow = millis();
      highduration = (millis() - starttimehigh);
      if (highduration < (2 * hightimesavg) || hightimesavg == 0) {
        hightimesavg = (highduration + hightimesavg + hightimesavg) / 3; // now we know avg dit time ( rolling 3 avg)
      }
      if (highduration > (5 * hightimesavg) ) {
        hightimesavg = highduration + hightimesavg;   // if speed decrease fast ..
      }
    }
  }

  // Now check the baud rate based on dit or dah duration either 1, 3 or 7 pauses
  if (filteredstate != filteredstatebefore) {
    stop = LOW;
    if (filteredstate == LOW) { // we did end on a HIGH
      if (highduration < (hightimesavg * 2) && highduration > (hightimesavg * 0.6)) { /// 0.6 filter out false dits
        strcat(CodeBuffer,".");
        //Serial.print(".");
      }
      if (highduration > (hightimesavg * 2) && highduration < (hightimesavg * 6)) {
        strcat(CodeBuffer,"-");
        //Serial.print("-");
        wpm = (wpm + (1200 / ((highduration) / 3))) / 2; //// the most precise we can do ;o)
      }
    }

    if (filteredstate == HIGH) { //// we did end a LOW
      float lacktime = 1;
      if (wpm > 25)lacktime = 1.0; ///  when high speeds we have to have a little more pause before new letter or new word
      if (wpm > 30)lacktime = 1.2;
      if (wpm > 35)lacktime = 1.5;
      if (lowduration > (hightimesavg * (2 * lacktime)) && lowduration < hightimesavg * (5 * lacktime)) { // letter space
        CodeToChar();
        CodeBuffer[0] = '\0';
        //AddCharacter('/');
        //Serial.print("/");
      }
      if (lowduration >= hightimesavg * (5 * lacktime)) { // word space
        CodeToChar();
        CodeBuffer[0] = '\0';
        AddCharacter(' ');
        Serial.print(" ");
      }
    }
  }
  if ((millis() - startttimelow) > (highduration * 6) && stop == LOW) {
    CodeToChar();
    CodeBuffer[0] = '\0';
    stop = HIGH;
  }
  // the end of main loop clean up//
  realstatebefore     = realstate;
  lasthighduration    = highduration;
  filteredstatebefore = filteredstate;
  display.drawString(0, 0, "WPM = "+String(wpm));
  display.drawString(64, 0,"BW = "+String(bw,0)+"Hz");
  display.setFont(ArialMT_Plain_16);
  display.drawString(0, 26, DisplayLine);
  display.display();
  display.setFont(ArialMT_Plain_10);
  display.clear();
}

void CodeToChar() { // translate cw code to ascii character//
  char decode_char = '{';
  if (strcmp(CodeBuffer,".-") == 0)      decode_char = char('a');
  if (strcmp(CodeBuffer,"-...") == 0)    decode_char = char('b');
  if (strcmp(CodeBuffer,"-.-.") == 0)    decode_char = char('c');
  if (strcmp(CodeBuffer,"-..") == 0)     decode_char = char('d'); 
  if (strcmp(CodeBuffer,".") == 0)       decode_char = char('e'); 
  if (strcmp(CodeBuffer,"..-.") == 0)    decode_char = char('f'); 
  if (strcmp(CodeBuffer,"--.") == 0)     decode_char = char('g'); 
  if (strcmp(CodeBuffer,"....") == 0)    decode_char = char('h'); 
  if (strcmp(CodeBuffer,"..") == 0)      decode_char = char('i');
  if (strcmp(CodeBuffer,".---") == 0)    decode_char = char('j');
  if (strcmp(CodeBuffer,"-.-") == 0)     decode_char = char('k'); 
  if (strcmp(CodeBuffer,".-..") == 0)    decode_char = char('l'); 
  if (strcmp(CodeBuffer,"--") == 0)      decode_char = char('m'); 
  if (strcmp(CodeBuffer,"-.") == 0)      decode_char = char('n'); 
  if (strcmp(CodeBuffer,"---") == 0)     decode_char = char('o'); 
  if (strcmp(CodeBuffer,".--.") == 0)    decode_char = char('p'); 
  if (strcmp(CodeBuffer,"--.-") == 0)    decode_char = char('q'); 
  if (strcmp(CodeBuffer,".-.") == 0)     decode_char = char('r'); 
  if (strcmp(CodeBuffer,"...") == 0)     decode_char = char('s'); 
  if (strcmp(CodeBuffer,"-") == 0)       decode_char = char('t'); 
  if (strcmp(CodeBuffer,"..-") == 0)     decode_char = char('u'); 
  if (strcmp(CodeBuffer,"...-") == 0)    decode_char = char('v'); 
  if (strcmp(CodeBuffer,".--") == 0)     decode_char = char('w'); 
  if (strcmp(CodeBuffer,"-..-") == 0)    decode_char = char('x'); 
  if (strcmp(CodeBuffer,"-.--") == 0)    decode_char = char('y'); 
  if (strcmp(CodeBuffer,"--..") == 0)    decode_char = char('z'); 
  
  if (strcmp(CodeBuffer,".----") == 0)   decode_char = char('1'); 
  if (strcmp(CodeBuffer,"..---") == 0)   decode_char = char('2'); 
  if (strcmp(CodeBuffer,"...--") == 0)   decode_char = char('3'); 
  if (strcmp(CodeBuffer,"....-") == 0)   decode_char = char('4'); 
  if (strcmp(CodeBuffer,".....") == 0)   decode_char = char('5'); 
  if (strcmp(CodeBuffer,"-....") == 0)   decode_char = char('6'); 
  if (strcmp(CodeBuffer,"--...") == 0)   decode_char = char('7'); 
  if (strcmp(CodeBuffer,"---..") == 0)   decode_char = char('8'); 
  if (strcmp(CodeBuffer,"----.") == 0)   decode_char = char('9'); 
  if (strcmp(CodeBuffer,"-----") == 0)   decode_char = char('0'); 

  if (strcmp(CodeBuffer,"..--..") == 0)  decode_char = char('?'); 
  if (strcmp(CodeBuffer,".-.-.-") == 0)  decode_char = char('.'); 
  if (strcmp(CodeBuffer,"--..--") == 0)  decode_char = char(','); 
  if (strcmp(CodeBuffer,"-.-.--") == 0)  decode_char = char('!'); 
  if (strcmp(CodeBuffer,".--.-.") == 0)  decode_char = char('@'); 
  if (strcmp(CodeBuffer,"---...") == 0)  decode_char = char(':'); 
  if (strcmp(CodeBuffer,"-....-") == 0)  decode_char = char('-'); 
  if (strcmp(CodeBuffer,"-..-.") == 0)   decode_char = char('/'); 

  if (strcmp(CodeBuffer,"-.--.") == 0)   decode_char = char('('); 
  if (strcmp(CodeBuffer,"-.--.-") == 0)  decode_char = char(')'); 
  if (strcmp(CodeBuffer,".-...") == 0)   decode_char = char('_'); 
  if (strcmp(CodeBuffer,"...-..-") == 0) decode_char = char('$'); 
  if (strcmp(CodeBuffer,"...-.-") == 0)  decode_char = char('>'); 
  if (strcmp(CodeBuffer,".-.-.") == 0)   decode_char = char('<'); 
  if (strcmp(CodeBuffer,"...-.") == 0)   decode_char = char('~'); 
  if (strcmp(CodeBuffer,".-.-") == 0)    decode_char = char('a'); // a umlaut
  if (strcmp(CodeBuffer,"---.") == 0)    decode_char = char('o'); // o accent
  if (strcmp(CodeBuffer,".--.-") == 0)   decode_char = char('a'); // a accent
  if (decode_char != '{') {
    AddCharacter(decode_char);
    Serial.print(decode_char);
  }
}

void AddCharacter(char newchar){
  for (int i = 0; i < num_chars; i++) DisplayLine[i] = DisplayLine[i+1];
  DisplayLine[num_chars] = newchar;
}

