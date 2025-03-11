# TagSyncNode
A ESP32-powered NAS (or just a localized file-sharing system, honestly, call it whatever you want) with an added attendance system using MFRC522.

Ever seen your school's IT admin or teacher manually copying files between computers every time your class submits an output with his/her flash drive? Painful, right? Well, TagSyncNode solves that.

All you need is:

✔ A breadboard or perfboard (as long as it conducts electricity, you're good)

✔ A MFRC522 RFID module

✔ An SD card module (with a SD Card of course, this would act as a shared SD card)

✔ Jumper wires or solid wires

✔ RGB Module (optional, but if you want to have it indicate it's status, the this is required)

✔ DS1302 RTC Module (If you want to have time tagging on the attendance)

With those, you can present this to the school IT admin or teacher to help them save time and energy. Also, this repo and premade HTML, you can chat with other people within the network, just type 'register:YourName' first to use this feature, and do not refresh your browser from that point on since you have to retype 'register:YourName' again but you can't use your name that you used earlier (that bug could be fixed if someone forked this). To make this work, wire that as follows:

RTC:

  CLK (Clock) → ESP32 GPIO14
  
  DAT (Data) → ESP32 GPIO13
  
  RST (Reset) → ESP32 GPIO12  
  
  
MFRC 522:

  RST → GPIO22
  
  SDA (CS) → GPIO21
  
  MOSI → GPIO23
  
  MISO → GPIO19
  
  SCK → GPIO18
  
  VCC/GND → 3.3V/GND
  

SD Card Module:

  CS → GPIO5 (or your chosen free pin)
  
  MOSI → GPIO23
  
  MISO → GPIO19
  
  SCK → GPIO18
  
  VCC/GND → 3.3V (or 5V)/GND  
  

RGB Module:

  Red → GPIO27
  
  Green → GPIO26
  
  Blue → GPIO25  
  

Disclaimer:

  Due to the limitations of ESP32 of maximum clients (which is 10), you can choose to connect this to a router with or without internet access and enable port forwarding for it on port 80 with it's default IP: 192.168.0.3. And depending on the ESP32 model (e.g. ESP-WROOM-32, ESP-WROVER-32 or ESP32-S3), this can only serve for X devices (depends on the ESP32 board you're using) and for the rest of the devices who is on this server will just receive the HTML and no dynamic (the chat feature) data.

P.S. 

  If you found this project helpful in any way, please consider donating via the GCash QR code provided to help me fund my future projects that I'll upload the source code and other required files to make that work and buy coffee to drink while making it. Any donation ammount is greatly appreciated. Thanks in advance :).
