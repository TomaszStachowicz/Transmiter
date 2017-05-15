/* DrippyT2 by Marek Macner (c) 2017 */
/* xBridgePlus protocol add by Tomasz Stachowicz */
#define DEBUG
//#define DEBUGM
//#define DEBUGX

#include <RFduinoBLE.h>
#include <SPI.h> 
#include <Stream.h>
#include "xBridgePlus.h"


#define PIN_SPI_SCK   4
#define PIN_SPI_MOSI  5
#define PIN_SPI_MISO  3
#define PIN_SPI_SS    6
#define PIN_IRQ       2 
#define MAX_FAILURES  10

//#define ALL_BYTES 0x1007   
//#define IDN_DATA 0x2001    
//#define SYSTEM_INFORMATION_DATA 0x2002  
//#define BATTERY_DATA 0x2005     

typedef struct  __attribute__((packed)) 
{
  uint8_t resultCode;
  uint8_t deviceID[13];
  uint8_t romCRC[2];
} IDNDataType;

typedef struct  __attribute__((packed)) 
{
  uint8_t uid[8];
  uint8_t resultCode;
  uint8_t responseFlags;
  uint8_t infoFlags;
  uint8_t errorCode;
  String sensorSN;
} SystemInformationDataType;

typedef struct  __attribute__((packed)) 
{
  bool sensorDataOK;
  String sensorSN;
  byte   sensorStatusByte;
  String sensorStatusString;
  byte  nextTrend;
  byte  nextHistory;
  uint16_t minutesSinceStart;
  uint16_t minutesHistoryOffset;
  uint16_t trend[16];
  uint16_t history[32];
} SensorDataDataType;

typedef struct  __attribute__((packed)) 
{
  long voltage;
  int voltagePercent;
  double temperatureC;
  double temperatureF;
  double rssi;
} RFDuinoDataType;

typedef struct  __attribute__((packed)) 
{
  uint8_t allBytes[345];
} AllBytesDataType;

typedef struct  __attribute__((packed)) 
{
  float voltage;
  float temperature;
} BatteryDataType;

typedef struct dataConfig 
{
  byte marker;
  byte protocolType;                  // 1 - LimiTTer, 2 - Transmiter, 3 - LibreCGM, 4 - Transmiter II, 10 - xBridgePlus
  byte runPeriod;                     // 0-9 main loop period in miutes, 0=on demenad 
  byte firmware;                      // firmware version starting 0x02
};

typedef enum {    
    UBP_TxFlagNone = 0 << 0,
    UBP_TxFlagIsRPC = 1 << 0,
    UBP_TxFlagRequiresACK = 1 << 1
    
} UBP_TxFlags;

bool UBP_isTxPending = false;
bool hostIsConnected = false;
extern void UBP_incomingChecksumFailed() __attribute__((weak));
extern void UBP_receivedPacket(unsigned short packetIdentifier, UBP_TxFlags txFlags, void *packetBuffer) __attribute__((weak));
extern void UBP_didAdvertise(bool start) __attribute__((weak));
extern void UBP_didConnect() __attribute__((weak));
extern void UBP_didDisconnect() __attribute__((weak));


byte resultBuffer[40]; 
byte dataBuffer[400];
byte NFCReady = 0;            // 0 - not initialized, 1 - initialized, no data, 2 - initialized, data OK
bool sensorDataOK = false;

IDNDataType idnData;
SystemInformationDataType systemInformationData;
SensorDataDataType sensorData;
RFDuinoDataType rfduinoData;
struct dataConfig valueSetup;
dataConfig *p;
AllBytesDataType allBytes;
BatteryDataType batteryData;

byte sensorDataHeader[24];
byte sensorDataBody[296];
byte sensorDataFooter[24];

byte noOfBuffersToTransmit = 1;
String TxBuffer[10];
String TxBuffer1 = "";

byte protocolType = 10;    // 1 - LimiTTer, 2 - Transmiter, 3 - Transmiter II, 10 - xBridgePlus
byte runPeriod = 15;       // czas w minutach - 0 = tylko na żądanie
byte  MY_FLASH_PAGE =  251;

bool BTconnected = false;
bool BatteryOK=true;

xBridgePlus xbridgeplus;

void setup() 
{
  p = (dataConfig*)ADDRESS_OF_PAGE(MY_FLASH_PAGE);
  Serial.begin(9600);
  #ifdef DEBUGM
    Serial.println("DrippyT2 - setup - start");
  #endif 
  
    setupInitData();//valueSetup.protocolType = 1; Limitter
  //timi DEBUG
  //protocolType = p->protocolType;
  //runPeriod = p->runPeriod;
  
  setupBluetoothConnection();//based on protocolType set customUUID, deviceName, advertisementData
  nfcInit();
  #ifdef DEBUGM
    Serial.println("nfcInit start");
  #endif 
  configWDT();
  xbridgeplus.sensor_data_is_current=false;
  xbridgeplus.sendBeacons();//get TXID, keepalive
}

void loop() 
{
  RFduino_ULPDelay(1000 * 60 * 7);//wait 7 minutes for xDrip requests
  if (BatteryOK && !xbridgeplus.sensor_data_is_current)
  {
    if(xbridgeplus.failure_count++ >MAX_FAILURES) RFduino_systemReset();//reboot if too many errors - workarround
    readAllData();
    RfduinoData();
    xbridgeplus.requested_data_packet=true;
    dataTransferBLE();
    xbridgeplus.sensor_data_is_current=false;//set invalid data
  }

  restartWDT();
}

