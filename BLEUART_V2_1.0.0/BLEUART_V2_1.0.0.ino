/* 
 Author      : Ed Nieuwenhuijs https://github.com/ednieuw , https://ednieuw.nl
 December 2024

Sketch for Bluetooth UART (universal asynchronous receiver / transmitter) communication with a BLE serial terminal.

A serial terminal can be installed on a PC (like Termite) or on a IOS or Android device
For IOS:  BLESerial nRF or BLEserial Pro   
For Android:  Serial Bluetooth Terminal

In this example entries in the serial terminal can be used to set an SSID, password and BLE broadcast name.

******
Compile with NimBLE version 2.x
******

------------------------------------------------------------------------------------------------
*/

#include <NimBLEDevice.h>     
//--------------------------------------------                                                //
// BLE   //#include <NimBLEDevice.h>
//--------------------------------------------
NimBLEServer *pServer      = NULL;
NimBLECharacteristic * pTxCharacteristic;
bool deviceConnected    = false;
bool oldDeviceConnected = false;
std::string ReceivedMessageBLE;

#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"                         // UART service UUID
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

//----------------------------------------
// Common
//----------------------------------------
 
#define   MAXTEXT 255
char      sptext[MAXTEXT]; 
bool UseBLELongString = false;                                                                // Set to true for faster communication on IOS devices
char SSID[30];                                                                                // 
char Password[40];                                                                            // 
char BLEbroadcastName[30];                                                                    // Name of the BLE beacon

//----------------------------------------
// Setup
//----------------------------------------
void setup() 
{
Serial.begin(115200);                                                                         // Setup the serial port to 115200 baud //
 int32_t Tick = millis(); 
 while (!Serial)  
 {if ((millis() - Tick) >5000) break;}  Tekstprintln("Serial started");                       // Wait max 5 seconds until serial port is started   
 strcpy(SSID,"");                                                                             // Default SSID
 strcpy(Password,"");                                                                         // Default password
 strcpy(BLEbroadcastName,"Edsoft");
 StartBLEService();     Tekstprintln("BLE started");                                          // Start BLE service

}
//----------------------------------------
// Loop
//----------------------------------------
void loop() 
{
 CheckDevices();
}
//--------------------------------------------                                                //
// COMMON Check connected input devices
//--------------------------------------------
void CheckDevices(void)
{
 CheckBLE();                                                                                  // Something with BLE to do?
 SerialCheck();                                                                               // Check serial port every second 
}

//--------------------------------------------                                                //
// COMMON check for serial input
//--------------------------------------------
void SerialCheck(void)
{
 String SerialString; 
 while (Serial.available())
    { 
     char c = Serial.read();                                                                  // Serial.write(c);
     if (c>31 && c<128) SerialString += c;                                                    // Allow input from Space - Del
     else c = 0;                                                                              // Delete a CR
    }
 if (SerialString.length()>0) 
    {
     ReworkInputString(SerialString);                                                        // Rework ReworkInputString();
     SerialString = "";
    }
}

//--------------------------------------------                                                //
// COMMON common print routines
//--------------------------------------------
void Tekstprint(char const *tekst)    { if(Serial) Serial.print(tekst);  SendMessageBLE(tekst); sptext[0]=0; } 
void Tekstprintln(char const *tekst)  { sprintf(sptext,"%s\n",tekst); Tekstprint(sptext); }
void TekstSprint(char const *tekst)   { Serial.print(tekst); sptext[0]=0;}                          // printing for Debugging purposes in serial monitor 
void TekstSprintln(char const *tekst) { sprintf(sptext,"%s\n",tekst); TekstSprint(sptext); }


//--------------------------------------------                                                //
//  COMMON Input from Bluetooth or Serial
//--------------------------------------------
void ReworkInputString(String InputString)
{
 if(InputString.length()> 40){Serial.printf("Input string too long (max40)\n"); return;}      // If garbage return
 for (int n=0; n<InputString.length()+1; n++)                                                 // remove CR and LF
       if (InputString[n] == 10 || InputString[n]==13) InputString.remove(n,1);
 sptext[0] = 0;                                                                               // Empty the sptext string
 
 if(InputString[0] > 31 && InputString[0] <127)                                               // Does the string start with a letter?
  { 
  switch (InputString[0])
   {
    case 'A':
    case 'a': 
            if (InputString.length() >4 && InputString.length() <30)
            {
             InputString.substring(1).toCharArray(SSID,InputString.length());
             sprintf(sptext,"SSID set: %s", SSID);  
            }
            else sprintf(sptext,"**** Length fault. Use between 4 and 30 characters ****");
            break;
    case 'B':
    case 'b':

             if (InputString.length() >4 && InputString.length() <40)
              {  
               InputString.substring(1).toCharArray(Password,InputString.length());
               sprintf(sptext,"Password set: %s\n Enter @ to reset ESP32 and connect to WIFI and NTP\n WIFI and NTP are turned ON", Password); 
              }
             else sprintf(sptext,"**** Length fault. Use between 5 and 40 characters ****");
             break;   
    case 'C':
    case 'c': 
   
             if (InputString.length() >4 && InputString.length() <30)
               {  
                InputString.substring(1).toCharArray(BLEbroadcastName,InputString.length());
                sprintf(sptext,"BLE broadcast name set: %s", BLEbroadcastName); 
              }
            else sprintf(sptext,"**** Length fault. Use between 4 and 30 characters ****");
            break;                                
    default: break;
    }
  }  
 Tekstprintln(sptext);                                   
 InputString = "";
}


// ********************** NimBLE code ************************************
//-----------------------------
// BLE  CheckBLE
//------------------------------
void CheckBLE(void)
{
 if(!deviceConnected && oldDeviceConnected)                                                   // Disconnecting
   {
    delay(300);                                                                               // Give the bluetooth stack the chance to get things ready
    pServer->startAdvertising();                                                              // Restart advertising
    TekstSprint("Start advertising\n");
    oldDeviceConnected = deviceConnected;
   }
 if(deviceConnected && !oldDeviceConnected)                                                   // Connecting
   { 
    oldDeviceConnected = deviceConnected;
   }
 if(ReceivedMessageBLE.length()>0)
   {
    SendMessageBLE(ReceivedMessageBLE);
    String BLEtext = ReceivedMessageBLE.c_str();
    ReceivedMessageBLE = "";
    ReworkInputString(BLEtext); 
   }
}

//--------------------------------------------                                                //
// BLE 
// SendMessage by BLE Slow in packets of 20 chars
// or fast in one long string.
// Fast can be used in IOS app BLESerial Pro
//------------------------------
void SendMessageBLE(std::string Message)
{
 if(deviceConnected) 
   {
    if (UseBLELongString)                                                                     // If Fast transmission (for Apple IOS) is possible
     {
      pTxCharacteristic->setValue(Message); 
      pTxCharacteristic->notify();
      delay(10);                                                                              // Bluetooth stack will go into congestion, if too many packets are sent
     } 
   else                                                                                       // Packets of max 20 bytes
     {   
      int parts = (Message.length()/20) + 1;
      for(int n=0;n<parts;n++)
        {   
         pTxCharacteristic->setValue(Message.substr(n*20, 20)); 
         pTxCharacteristic->notify(); 
         delay(10);                                                                           // Bluetooth stack will go into congestion, if too many packets are sent
        }
     }
   } 
}
//--------------------------------------------                                                //
// BLE Start BLE Classes
//------------------------------
class MyServerCallbacks: public NimBLEServerCallbacks 
{
 void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {deviceConnected = true;Serial.println("Connected" ); };
 void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {deviceConnected = false;Serial.println("NOT Connected" );}
};
  
class MyCallbacks: public NimBLECharacteristicCallbacks 
{
 void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo& connInfo) override  
  {
   std::string rxValue = pCharacteristic->getValue();
   ReceivedMessageBLE = rxValue + "\n";
  //  if (rxValue.length() > 0) {for (int i = 0; i < rxValue.length(); i++) printf("%c",rxValue[i]); }
  //  printf("\n");
  }  
};

//--------------------------------------------                                                //
// BLE Start BLE Service
//------------------------------
void StartBLEService(void)
{
 NimBLEDevice::init(BLEbroadcastName);                                                        // Create the BLE Device
 pServer = NimBLEDevice::createServer();                                                      // Create the BLE Server
 pServer->setCallbacks(new MyServerCallbacks());
 BLEService *pService = pServer->createService(SERVICE_UUID);                                 // Create the BLE Service
 pTxCharacteristic                     =                                                      // Create a BLE Characteristic 
     pService->createCharacteristic(CHARACTERISTIC_UUID_TX, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);                 
 NimBLECharacteristic * pRxCharacteristic = 
     pService->createCharacteristic(CHARACTERISTIC_UUID_RX, NIMBLE_PROPERTY::WRITE);
 pRxCharacteristic->setCallbacks(new MyCallbacks());
 pService->start(); 
 BLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
 pAdvertising->addServiceUUID(SERVICE_UUID); 
 pAdvertising->setName(BLEbroadcastName);    
 pServer->start();                                                                            // Start the server  Nodig??
 pServer->getAdvertising()->start();                                                          // Start advertising
 TekstSprint("BLE Waiting a client connection to notify ...\n"); 
}
//                                                                                            //


