/*
Arduino sketch which plays video on Nokia 3310 monochrome LCDs via Arduino from an external SD card
See https://joeraut.com/blog/playing-video-nokia-3310/ for more info
*/


#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include <SD.h>

File file, root; // file = the file of the video to be played; root = the file that lists all the other files :P

// pin 7 - Serial clock out (SCLK)
// pin 6 - Serial data out (DIN)
// pin 5 - Data/Command select (D/C)
// pin 4 - LCD chip select (CS)
// pin 3 - LCD reset (RST)
Adafruit_PCD8544 display = Adafruit_PCD8544(7, 6, 5, 4, 3);

String fileName; // name of the chosen video file

byte x,y = 0; // 
byte fps = 0; // stores the current frame rate
long startTime = 0; // used for calculating the FPS
boolean debounce, debounce2, firstRun = true;
boolean showFPS = false;

void setup() {

  //Serial.begin(9600);

  display.begin();
  display.setContrast(53);
  display.setTextColor(BLACK, WHITE);
  display.clearDisplay();
  display.display();

  if (!SD.begin(4)) { // there's no SD card detected or SD card error
    display.clearDisplay();
    display.print("Insert SD Card");
    display.display();
    delay(1000);
    asm("jmp 0"); // 'reboots' the Arduino by jumping to 0x00. Can cause problems in some cases, but works fine here.
    return;
  }

  root = SD.open("/"); // open the root of the SD card for listing fies

  while(1) {

    if((readCapacitivePin(19) > 3 && debounce) || firstRun) { // Select file button pressed
      debounce = false;
      firstRun = false; // if program just started, run this function once (to select the first file and display something on the screen)
      while(1) { // look for valid files
        fileName = getFileName(root); // get current filename of what is being read
        String newFileName = "";
        for(int i=0; i<fileName.length(); i++) {
          if(fileName[i] != '~') { // replace all '~'s with nothing, due to a bug with _'s in a filename appearing as _~'s.
            newFileName += fileName[i];
          }
        }
        fileName = newFileName;
        byte len = fileName.length(); // get length of filename
        if(fileName[len-3] == 'D' && fileName[len-2] == 'A' && fileName[len-1] == 'T') { // does the file have .dat extension?
          fileName.toLowerCase(); // make it lower case (files are listed upper case, looks ugly)
          break; // found a valid file (.dat extension) and carry on with rest of program
        }
      }
      delay(100); // stop pressing the button more than once within 100ms, basic debouncing i.e. 'accidental press preventation'
    }
    if(!(readCapacitivePin(19) > 3)) { // let go of the Select file button, you can now press it again
      debounce = true;
    }

    if(readCapacitivePin(18) > 3 && debounce2) { // pressed Toggle FPS button
      debounce2 = false;
      showFPS = !showFPS; // toggle FPS
      delay(100); // stop pressing the button more than once within 100ms, basic debouncing i.e. 'accidental press preventation'
    }
    if(!(readCapacitivePin(18) > 3)) { // let go of the Toggle FPS, you can now press it again
      debounce2 = true;
    }

    display.clearDisplay();
    writeString(6, 0, "Config", 2); // title
    display.drawLine(0, 17, 83, 17, BLACK);

    writeString(0, 19, fileName, 1);
    writeString(0, 29, (showFPS)?"Show FPS: YES":"Show FPS: NO", 1);

    display.drawLine(0, 39, 83, 39, BLACK);
    writeStringInverse(0, 40, "SEL   FPS   OK", 1);

    if(readCapacitivePin(19) > 3) {
      display.fillRect(0, 39, 17, 9, WHITE);
    }
    if(readCapacitivePin(18) > 3) {
      display.fillRect(36, 39, 17, 9, WHITE);
    }
    display.display();
    
    // debugging - shows the readings of the capacitive buttons.
    /*Serial.print(readCapacitivePin(19));
    Serial.print(' ');
    Serial.print(readCapacitivePin(18));
    Serial.print(' ');
    Serial.println(readCapacitivePin(17));*/
    
    if(readCapacitivePin(17) > 3) { // pressed the Play button
      display.clearDisplay();
      display.display();
      break; // carry on with program
    }
  }
}

void loop() {

  // converts file name String to char array, as SD.open() can't use Strings.
  String dataFileName = fileName;
  char __dataFileName[sizeof(dataFileName)+3];
  dataFileName.toCharArray(__dataFileName, sizeof(__dataFileName)+3);

  file = SD.open(__dataFileName);

  while(file.available()) {
    char ch = file.read();
    switch(ch) {
    case B000000...B111111: // character contains graphics
      display.drawPixel(x, y, ch & B100000);
      x++;
      display.drawPixel(x, y, ch & B010000);
      x++;
      display.drawPixel(x, y, ch & B001000);
      x++;
      display.drawPixel(x, y, ch & B000100);
      x++;
      display.drawPixel(x, y, ch & B000010);
      x++;
      display.drawPixel(x, y, ch & B000001);
      x++;
      break;
    case 123: // new line command
      x=0;
      y++;
      break;
    case 124: // reset character positions
      x = 0;
      y = 0;
      display.clearDisplay();
      break;
    case 125: // send framebuffer image to display and display (displays the image, basically)
      if(showFPS) { // if showFPS is enabled, show the FPS!
        fps = 1000/(millis()-startTime);
        display.setTextColor(BLACK, WHITE);
        display.println(fps);
        //display.print(freeRam());
        startTime = millis();
      }
      display.display();
      break;
    }
  }
  file.close(); // finished the file, so we close it, then it starts playing again and again...
}

uint8_t readCapacitivePin(int pinToMeasure) { // function to read capacitive button
  volatile uint8_t* port;
  volatile uint8_t* ddr;
  volatile uint8_t* pin;
  byte bitmask;
  port = portOutputRegister(digitalPinToPort(pinToMeasure));
  ddr = portModeRegister(digitalPinToPort(pinToMeasure));
  bitmask = digitalPinToBitMask(pinToMeasure);
  pin = portInputRegister(digitalPinToPort(pinToMeasure));
  *port &= ~(bitmask);
  *ddr  |= bitmask;
  delay(1);
  *ddr &= ~(bitmask);
  *port |= bitmask;
  uint8_t cycles = 17;
  if (*pin & bitmask) { 
    cycles =  0;
  }
  else if (*pin & bitmask) { 
    cycles =  1;
  }
  else if (*pin & bitmask) { 
    cycles =  2;
  }
  else if (*pin & bitmask) { 
    cycles =  3;
  }
  else if (*pin & bitmask) { 
    cycles =  4;
  }
  else if (*pin & bitmask) { 
    cycles =  5;
  }
  else if (*pin & bitmask) { 
    cycles =  6;
  }
  else if (*pin & bitmask) { 
    cycles =  7;
  }
  else if (*pin & bitmask) { 
    cycles =  8;
  }
  else if (*pin & bitmask) { 
    cycles =  9;
  }
  else if (*pin & bitmask) { 
    cycles = 10;
  }
  else if (*pin & bitmask) { 
    cycles = 11;
  }
  else if (*pin & bitmask) { 
    cycles = 12;
  }
  else if (*pin & bitmask) { 
    cycles = 13;
  }
  else if (*pin & bitmask) { 
    cycles = 14;
  }
  else if (*pin & bitmask) { 
    cycles = 15;
  }
  else if (*pin & bitmask) { 
    cycles = 16;
  }
  *port &= ~(bitmask);
  *ddr  |= bitmask;

  return cycles;
}

int freeRam () {
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}

String getFileName(File selector) { // function to get then name of the next file, for file list
  File selectorEntry = root.openNextFile();
  if(!selectorEntry) { // we've reached the last file on the card
    root.rewindDirectory(); // start reading from the beginning again
    selectorEntry = root.openNextFile();
  }
  String selectorName = selectorEntry.name();
  selectorEntry.close(); // close the read file, otherwise it wastes LOADS of ram!
  return selectorName;
}
void writeString(byte writerX, byte writerY, String writerString, byte fontSize) { // just a shortcut to print a string to the display
  display.setCursor(writerX, writerY);
  display.setTextSize(fontSize);
  display.setTextColor(BLACK);
  display.print(writerString);
}
void writeStringInverse(byte writerX, byte writerY, String writerString, byte fontSize) { // just a shortcut to print a string to the display, but white letters on a black background
  display.setCursor(writerX, writerY);
  display.setTextSize(fontSize);
  display.setTextColor(WHITE, BLACK);
  display.print(writerString);
}