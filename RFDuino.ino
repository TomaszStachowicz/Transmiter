void configSPI()
{
  pinMode(PIN_SPI_SS, OUTPUT);
  pinMode(PIN_IRQ, OUTPUT); 
  SPI.begin();
  SPI.setDataMode(SPI_MODE0);
  SPI.setBitOrder(MSBFIRST);
  SPI.setFrequency(1000); 
}

void configWDT()
{
  NRF_WDT->CONFIG = (WDT_CONFIG_SLEEP_Run << WDT_CONFIG_SLEEP_Pos) | (WDT_CONFIG_HALT_Pause << WDT_CONFIG_HALT_Pos); 
  NRF_WDT->CRV = 32768 * 60 * 11;         // 11 minut
  NRF_WDT->RREN |= WDT_RREN_RR0_Msk;                      
  NRF_WDT->TASKS_START = 1; 
}

void restartWDT()
{
   NRF_WDT->RR[0] = WDT_RR_RR_Reload;
}

void RfduinoData() 
{
//  analogReference(VBG);                
//  analogSelection(VDD_1_3_PS);
//  delay(100);         
//  int sensorValue = analogRead(1); 
  int sensorValue = 1023; 
  rfduinoData.voltage = sensorValue * (3.6 / 1023.0)*1000; //mV
  rfduinoData.voltagePercent = map(rfduinoData.voltage, 2400, 3600, 0, 100);  
//  rfduinoData.temperatureC = RFduino_temperature(CELSIUS); 
//  rfduinoData.temperatureF = RFduino_temperature(FAHRENHEIT);  
  if (rfduinoData.voltage < 2400) BatteryOK = false;
  else BatteryOK = true;
  //timi
  xbridgeplus.data_packet.dex_battery=rfduinoData.voltage;
  xbridgeplus.data_packet.my_battery=rfduinoData.voltagePercent;

  #ifdef DEBUGM
    Serial.println("RFDuino data:");
    Serial.print(" - Voltage [mv]: ");
    Serial.println(rfduinoData.voltage);
    Serial.print(" - Voltage [%]: ");
    Serial.println(rfduinoData.voltagePercent);
    Serial.print(" - Temperature [C]: ");
    Serial.println(rfduinoData.temperatureC);
    Serial.print(" - Temperature [F]: ");
    Serial.println(rfduinoData.temperatureF);
  #endif
}

void readAllData()
{  
  if (BatteryOK==false)return;//if battery is low than exit
  NFC_wakeUP();
  NFC_CheckWakeUpEventRegister();
  NFCReady = 0;
  SetNFCprotocolCommand();
  
  runSystemInformationCommandUntilNoError(10);
  systemInformationData = systemInformationDataFromGetSystemInformationResponse();
  printSystemInformationData(systemInformationData);
  sensorData.sensorDataOK = readSensorData();
  if(sensorData.sensorDataOK)xbridgeplus.sensor_data_is_current=true;

  decodeSensor();
  RfduinoData();

  sendNFC_ToHibernate();
}

void setupInitData()
{
  if (p->marker == 'T')
  {
    //timi DEBUG
    //protocolType = p->protocolType;
    //runPeriod = p->runPeriod;
    #ifdef DEBUG
      Serial.print("Init data present at page ");
      Serial.println(MY_FLASH_PAGE);
      Serial.print("  protocolType = ");
      Serial.println(p->protocolType);
      Serial.print("  runPeriod = ");
      Serial.println(p->runPeriod);
      Serial.print("  firmware = ");
      Serial.println(p->firmware);
    #endif
  }
  else
  {
    //eraseData();
    valueSetup.marker = 'T';
    //timi DEBUG
    //valueSetup.protocolType = 1;                  // 1 - LimiTTer, 2 - Transmiter, 3 - LibreCGM, 4 - Transmiter II, 10 - xBridgePlus
    valueSetup.runPeriod = 5; 
    valueSetup.firmware = 0x02;  
    //writeData();
    //timi DEBUG
    //protocolType = p->protocolType;
    //runPeriod = p->runPeriod;
    #ifdef DEBUG
      Serial.print("New init data stored at page ");
      Serial.println(MY_FLASH_PAGE);
      Serial.print("  protocolType = ");
      Serial.println(p->protocolType);
      Serial.print("  runPeriod = ");
      Serial.println(p->runPeriod, HEX);
      Serial.print("  firmware = ");
      Serial.println(p->firmware, HEX);
    #endif
  }    
}

void eraseData()
{
  int rc;
  #ifdef DEBUGM
    Serial.print("Attempting to erase flash page ");
    Serial.print(MY_FLASH_PAGE);
  #endif  
  rc = flashPageErase(PAGE_FROM_ADDRESS(p));
  #ifdef DEBUGM
    if (rc == 0)
      Serial.println(" -> Success");
    else if (rc == 1)
      Serial.println(" -> Error - the flash page is reserved");
    else if (rc == 2)
      Serial.println(" -> Error - the flash page is used by the sketch");
  #endif
}

void writeData()
{
  int rc;
  #ifdef DEBUGM
    Serial.print("Attempting to write data to flash page ");
    Serial.print(MY_FLASH_PAGE);
  #endif   
  valueSetup.marker = 'T';
  rc = flashWriteBlock(p, &valueSetup, sizeof(valueSetup));
  #ifdef DEBUG
    if (rc == 0)
      Serial.println(" -> Success");
    else if (rc == 1)
      Serial.println(" -> Error - the flash page is reserved");
    else if (rc == 2)
      Serial.println(" -> Error - the flash page is used by the sketch");
  #endif 
}

void displayData()
{
  #ifdef DEBUGM
    Serial.print("The data stored in flash page ");
    Serial.print(MY_FLASH_PAGE);
    Serial.println(" contains: ");
    Serial.print("  protocolType = ");
    Serial.println(p->protocolType);
    Serial.print("  runPeriod = ");
    Serial.println(p->runPeriod);
    Serial.print("  firmware = ");
    Serial.println(p->firmware, HEX);
  #endif 
}

void RFduinoBLE_onReceive(char *data, int len) 
{
    #ifdef DEBUG
      Serial.println("RFduinoBLE_onReceive: packet received.");
    #endif
  if (data[0] == 'V')
  {
    String v = "v ";
    v += String(p->protocolType) + " ";
    v += String(p->runPeriod) + " ";
    v += String(p->firmware, HEX) + " v";
    #ifdef DEBUG
      Serial.println("V-command received.");
      Serial.println(v);
    #endif
    RFduinoBLE.send(v.c_str(), v.length());   
    displayData();
  }
  else if (data[0] == 'M')
  {    
    valueSetup.protocolType = (byte) (data[1]- '0');  
    valueSetup.runPeriod = (byte) (data[2]- '0');
    valueSetup.firmware = p->firmware; 
    #ifdef DEBUG
      Serial.println("M-command received.");
    #endif    
    while(!RFduinoBLE.radioActive){} 
    delay(6);
    eraseData();
    writeData();
    displayData();  
    protocolType = p->protocolType;
    runPeriod = p->runPeriod;
  }
  else if (data[0] == 'R')
  {
    #ifdef DEBUG
      Serial.println("R-command received.");
    #endif  
    readAllData();
    dataTransferBLE();
  }
  else
  {
    switch(data[1]){//xBridge packet:

    case 0x01:  //if TXID packet is received: len==6 and cmd_code==01
      #ifdef DEBUG
          Serial.print("TXID received: ");
      #endif
      if(data[0]!=6)return;
      memcpy(&xbridgeplus.beacon_packet.dex_src_id,data+2,sizeof(xbridgeplus.beacon_packet.dex_src_id));
      #ifdef DEBUG
          Serial.println(long(xbridgeplus.beacon_packet.dex_src_id));
      #endif
    break;

    case 0xF0:  //if ACK packet is received: len==2 and cmd_code==0xF1
      if(data[0]!=2)return;
      xbridgeplus.received_ack_packet=true;
      #ifdef DEBUG
          Serial.println("ACK received.");
      #endif
    break;

    case 0x02:  //xBridgePlus packet:
        switch(data[2]){//parse xBridgePlus sub-codes:
          case 0x00:
            //if data_request_packet packet is received: len==12 and sub_code==00 cmd_code==02 Check requested_sub_code in next byte:
            //0x00 data_packet, 0x01 status_packet, 0x02  last 15 minutes readings part1, 0x03 last 15 minutes readings part2, 0x04 last 8 hours readings part1, 0x05 last 8 hours readings part2
            switch(data[3]){
            // xBridge data_packet is requested:
            case 0x00:
            #ifdef DEBUG
              Serial.println("data_packet reqested.");
            #endif
            xbridgeplus.requested_data_packet=true;
            readAllData();
            dataTransferBLE();
            break;

            //last 15 minutes readings - part1 (current and last 7 readings, 1 minute interval) is requested:
            case 0x02:
            #ifdef DEBUG
              Serial.println("last 15 minutes readings part1 reqested.");
            #endif
            //if(data[0]!=12)return;
            xbridgeplus.requested_quarter_packet1=true;
            readAllData();              
            dataTransferBLE();
            break;
            
            //buffer[1] == 0x02 && buffer[2] == 0x3 get last 15 minutes readings - part1 (next 8 readings, 1 minute interval)
            case 0x03:
            #ifdef DEBUG
              Serial.println("last 15 minutes readings part2 reqested.");
            #endif
            //if(data[0]!=12)return;
            xbridgeplus.requested_quarter_packet2=true;
            readAllData();
            dataTransferBLE();
            break;
     
            default://bad request packet
            #ifdef DEBUG
              Serial.println("ERROR: xBridgePlus unsupported request packet received.");
              Serial.print("data-1: ");
              Serial.print(int(data[1]));
              Serial.print("data-2: ");
              Serial.print(int(data[2]));
              Serial.print("data-3: ");
              Serial.println(int(data[3]));
            #endif
            break;      
          }//switch requested_sub_code
          break;
 
        default://bad xBridgePlus packet
        #ifdef DEBUG
          Serial.println("ERROR: xBridgePlus unsupported packet received.");
          Serial.print("data-1: ");
          Serial.print(int(data[1]));
          Serial.print("data-2: ");
          Serial.println(int(data[2]));
        #endif
        break;      
      }//switch sub_code
    break;//xBridgePlus request packet received
    
    default://bad xBridge packet
        #ifdef DEBUG
          Serial.println("ERROR: unknown packet received,");
          Serial.print("data-1: ");
          Serial.print(int(data[1]));
          Serial.print("data-2: ");
          Serial.println(int(data[2]));
        #endif
    break;      
    } //switch xBridgePlus cmd_code
  }  //not Transmiter packet    
}

void RFduinoBLE_onDisconnect() 
{
    BTconnected = false;
    #ifdef DEBUG
      Serial.println("BT disconnected.");
    #endif
//    _UBP_hostDisconnected();
}

void RFduinoBLE_onConnect() 
{
    BTconnected = true;
    #ifdef DEBUG
      Serial.println("BT connected.");
    #endif
    hostIsConnected = true;
    if (UBP_didConnect) UBP_didConnect();
}

void RFduinoBLE_onRSSI(int rssi)
{
  rfduinoData.rssi = rssi;
}

void RFduinoBLE_onAdvertisement(bool start) {
    if (UBP_didAdvertise) UBP_didAdvertise(start);
}

bool BLEconnected()
{
  return hostIsConnected;
}
void dataTransferBLE()
{  
  
  for (int i=0; i<6; i++)
  {
    if (BTconnected)
    {
      #ifdef DEBUGM
        Serial.println("Conected :");
      #endif
      if (protocolType == 1) forLimiTTer();
      else if (protocolType == 2) forTransmiter1();
      else if (protocolType == 3) forTransmiter2(); 
      else if (protocolType == 4) forLibreCGM();
      else if (protocolType == 10) forxBdridgePlus();
      #ifdef DEBUGM
        Serial.println("Data transferred.");
      #endif
      //break;
    }
    else
    {     
      #ifdef DEBUGM
        Serial.print("Not conected - data not transferred -> try:");
        //Serial.println(i);
      #endif
      //delay(1000);
      RFduino_ULPDelay(1000);
    }
  }  
  //NFCReady = 1;
}

void setupBluetoothConnection() 
{
  if (protocolType == 1) RFduinoBLE.deviceName = "LimiTTer";
  else if (protocolType == 2) RFduinoBLE.deviceName = "LimiTTer";
  else if (protocolType == 3) RFduinoBLE.deviceName = "Transmiter";   
  RFduinoBLE.advertisementData = "data";
  RFduinoBLE.customUUID = "c97433f0-be8f-4dc8-b6f0-5343e6100eb4";
  RFduinoBLE.advertisementInterval = MILLISECONDS(300); 

  if (protocolType == 10) {
    RFduinoBLE.deviceName = "xBridge02";   
    RFduinoBLE.advertisementData = "rfduino";
    RFduinoBLE.customUUID = "0000ffe0-0000-1000-8000-00805f9b34fb";
    RFduinoBLE.txPowerLevel = -8;  // (-20dbM to +4 dBm: -20, -16, -12, -8, -4, 0, +4 - MAX)
    RFduinoBLE.advertisementInterval = MILLISECONDS(200); //interval between advertisement transmissions ms (range is 20ms to 10.24s) - default 20ms
  }

  
  RFduinoBLE.txPowerLevel = 4; 
  RFduinoBLE.begin();    
  #ifdef DEBUGM
    Serial.println("... done seting up Bluetooth stack and starting connection.");
  #endif  
}


//send a beacon with the TXID
void xBridgePlus::sendBeacon(){
  xbridgeplus.beacon_packet.size=0x7;
  xbridgeplus.beacon_packet.cmd_code=0xF1;
  //xbridgeplus.beacon_packet.dex_src_id=0x0;
  xbridgeplus.beacon_packet.function= DEXBRIDGE_PROTO_LEVEL;
  RFduinoBLE.send((char*)&xbridgeplus.beacon_packet, xbridgeplus.beacon_packet.size);
  #ifdef DEBUG
      Serial.print("BLE send BEACON_PACKET: ");
      Serial.println((long)xbridgeplus.beacon_packet.dex_src_id);
  #endif  
}

//repeat sending beacons until TXID is received, default timeout=4minutes
void xBridgePlus::sendBeacons() {
  int timeout=4*60;
    if(protocolType=10)
    do{
      if (!BTconnected){//sleep if no BLE connection
        RFduino_ULPDelay(1000 * 60);
        timeout=timeout-60;
      }
    
      sendBeacon();
      RFduino_ULPDelay(1000 * 5);
      timeout=timeout-5;
    }while(xbridgeplus.beacon_packet.dex_src_id==0 && timeout>=0);
}

