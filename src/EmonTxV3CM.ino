/*
  emonTxV3.4 Continuous Sampling
  using EmonLibCM https://github.com/openenergymonitor/EmonLibCM
  Authors: Robin Emley, Robert Wall, Trystan Lea
  
  -----------------------------------------
  Part of the openenergymonitor.org project
  Licence: GNU GPL V3
*/

/*
Change Log:
v1.0: First release of EmonTxV3 Continuous Monitoring Firmware.
v1.1: First stable release, Set default node to 15
v1.2: Enable RF startup test sequence (factory testing), Enable DEBUG by default to support EmonESP
v1.3: Inclusion of watchdog

emonhub.conf node decoder (nodeid is 8 when switch is off, 9 when switch is on)
See: https://github.com/openenergymonitor/emonhub/blob/emon-pi/configuration.md
copy the following in to emonhub.conf:

[[15]]
  nodename = EmonTxV3CM_15
  [[[rx]]]
    names = MSG, Vrms, P1, P2, P3, P4, E1, E2, E3, E4, T1, T2, T3, pulse
    datacodes = L,h,h,h,h,h,L,L,L,L,h,h,h,L
    scales = 1,0.01,1,1,1,1,1,1,1,1,0.01,0.01,0.01,1
    units = n,V,W,W,W,W,Wh,Wh,Wh,Wh,C,C,C,p
    whitening = 1

*/
#include <Arduino.h>
#include <avr/wdt.h>

const byte version = 13;                                 // Firmware version divide by 10 to get version number e,g 05 = v0.5

// Comment/Uncomment as applicable
#define ENABLE_RF                                       // Enable RF69 transmit, turn off if using direct serial, or EmonESP
#define RF_WHITENING                                    // Improves rfm reliability
#define PRINT_DATA                                      // Print data in Key:Value format to serial, used by EmonESP & emonhub EmonHubTx3eInterfacer
#define DEBUG                                        // Debug level print out
// #define SHOW_CAL                                     // Uncomment to show current for calibration

#define RF69_COMPAT 1                                   // Set to 1 if using RFM69CW, or 0 if using RFM12B

#include "emonLibCM.h"
#include <JeeLib.h>                                     // https://github.com/jcw/jeelib - Tested with JeeLib 10 April 2017
// ISR(WDT_vect) { Sleepy::watchdogEvent(); }

byte RF_freq = RF12_433MHZ;                             // Frequency of radio module can be RF12_433MHZ, RF12_868MHZ or RF12_915MHZ.
byte nodeID = 15;                                        // node ID for this emonTx.
int networkGroup = 210;                                 // wireless network group, needs to be same as emonBase / emonPi and emonGLCD. OEM default is 210

typedef struct {
    unsigned long Msg;
    int Vrms,P1,P2,P3,P4;
    unsigned long E1,E2,E3,E4;
    int T1,T2,T3;
    unsigned long pulse;
} PayloadTX;
PayloadTX emontx;                                       // create an instance
static void showString (PGM_P s);
 
DeviceAddress allAddresses[3];                          // Array to receive temperature sensor addresses
/* Example - how to define temperature sensors, prevents an automatic search
DeviceAddress allAddresses[3] = {
    {0x28, 0x81, 0x43, 0x31, 0x7, 0x0, 0x0, 0xD9},
    {0x28, 0x8D, 0xA5, 0xC7, 0x5, 0x0, 0x0, 0xD5},      // Use the actual addresses, as many as required
    {0x28, 0xC9, 0x58, 0x32, 0x7, 0x0, 0x0, 0x89}       // up to a maximum of 6
};
*/
int allTemps[3];                                        // Array to receive temperature measurements

//----------------------------emonTx V3 Settings-------------------------------------------------
float i1Cal = 90.9;    // (2000 turns / 22 Ohm burden) = 90.9
float i1Lead = 4.2;
float i2Cal = 90.9;    // (2000 turns / 22 Ohm burden) = 90.9
float i2Lead = 4.2;
float i3Cal = 90.9;    // (2000 turns / 22 Ohm burden) = 90.9
float i3Lead = 4.2;
float i4Cal = 16.67;   // (2000 turns / 120 Ohm burden) = 16.67
float i4Lead = 1.0;
float vCal  = 268.97;  // (240V x 13) / 11.6V = 268.97 Calibration for UK AC-AC adapter 77DB-06-09
const float vCal_USA    = 130.0;   // Calibration for US AC-AC adapter 77DA-10-09
bool USA=false;

//----------------------------emonTx V3 hard-wired connections-----------------------------------
const byte LEDpin      = 6;  // emonTx V3 LED
const byte DS18B20_PWR = 19; // DS18B20 Power
const byte DIP_switch1 = 8;  // RF node ID (default no chance in node ID, switch on for nodeID -1) switch off D8 is HIGH from internal pullup
const byte DIP_switch2 = 9;  // Voltage selection 240 / 110 V AC (default switch off 240V)  - switch off D9 is HIGH from internal pullup

//---------------------------------CT availability status----------------------------------------
byte CT_count = 0;
bool CT1, CT2, CT3, CT4; // Record if CT present during startup

unsigned long start = 0;

//----------------------------------------Setup--------------------------------------------------
void setup()
{
  wdt_enable(WDTO_8S);

  pinMode(LEDpin, OUTPUT);
  digitalWrite(LEDpin,HIGH);
  
  pinMode(DS18B20_PWR, OUTPUT);
  digitalWrite(DS18B20_PWR, HIGH);
  
  pinMode(DIP_switch1, INPUT_PULLUP);
  pinMode(DIP_switch2, INPUT_PULLUP);
  
  // Serial---------------------------------------------------------------------------------
  Serial.begin(115200);
  
  // ---------------------------------------------------------------------------------------
  if (digitalRead(DIP_switch1)==LOW) nodeID++;                            // IF DIP switch 1 is switched on (LOW) then add 1 from nodeID

  #ifdef DEBUG
    Serial.print(F("emonTx V3.4 EmonLibCM Continuous Monitoring V")); Serial.println(version*0.1);
    Serial.println(F("OpenEnergyMonitor.org"));
  #else
    Serial.println(F("describe:EmonTX3CM"));
  #endif
  
  load_config(true);                                                     // Load RF config from EEPROM  true = verbose 
  
  #ifdef ENABLE_RF
    #ifdef DEBUG
      #if (RF69_COMPAT)
        Serial.print(F("RFM69CW"));
      #else
        Serial.print(F("RFM12B"));
      #endif
      Serial.print(F(" Node: ")); Serial.print(nodeID);
      Serial.print(" Freq: ");
      if (RF_freq == RF12_433MHZ) Serial.print(F("433MHz"));
      if (RF_freq == RF12_868MHZ) Serial.print(F("868MHz"));
      if (RF_freq == RF12_915MHZ) Serial.print(F("915MHz"));
      Serial.print(F(" Group: ")); Serial.println(networkGroup);
      Serial.println(" ");
    #endif
  #endif
  
  // Read status of USA calibration DIP switch----------------------------------------------
  if (digitalRead(DIP_switch2)==LOW) USA=true;                            // IF DIP switch 2 is switched on then activate USA mode

  // Check connected CT sensors ------------------------------------------------------------
  if (analogRead(1) > 0) {CT1 = 1; CT_count++;} else CT1=0;               // check to see if CT is connected to CT1 input, if so enable that channel
  if (analogRead(2) > 0) {CT2 = 1; CT_count++;} else CT2=0;               // check to see if CT is connected to CT2 input, if so enable that channel
  if (analogRead(3) > 0) {CT3 = 1; CT_count++;} else CT3=0;               // check to see if CT is connected to CT3 input, if so enable that channel
  if (analogRead(4) > 0) {CT4 = 1; CT_count++;} else CT4=0;               // check to see if CT is connected to CT4 input, if so enable that channel
  if ( CT_count == 0) CT1=1;                                              // If no CT's are connected CT1-4 then by default read from CT1

  // ---------------------------------------------------------------------------------------
  #ifdef ENABLE_RF
    rf12_initialize(nodeID, RF_freq, networkGroup);                       // initialize RFM12B/rfm69CW
    for (int i=10; i>=0; i--)                                             // Send RF test sequence (for factory testing)
    {
      emontx.P1=i;

      PayloadTX tmp = emontx;
      #ifdef RF_WHITENING
          byte WHITENING = 0x55;
          for (byte i = 0, *p = (byte *)&tmp; i < sizeof tmp; i++, p++)
              *p ^= (byte)WHITENING;
      #endif
      rf12_sendNow(0, &tmp, sizeof tmp);
      delay(100);
    }
    rf12_sendWait(2);
    emontx.P1=0;
  #endif
  
  // ---------------------------------------------------------------------------------------
  
  readInput();
  digitalWrite(LEDpin,LOW);

  #ifdef DEBUG
    if (CT_count==0) {
      Serial.println(F("NO CT's detected"));
    } else {
      if (CT1) { Serial.print(F("CT1 detected, i1Cal:")); Serial.println(i1Cal); }
      if (CT2) { Serial.print(F("CT2 detected, i2Cal:")); Serial.println(i2Cal); }
      if (CT3) { Serial.print(F("CT3 detected, i3Cal:")); Serial.println(i3Cal); }
      if (CT4) { Serial.print(F("CT4 detected, i4Cal:")); Serial.println(i4Cal); }
    }
    delay(200);
  #endif

  // ----------------------------------------------------------------------------
  // EmonLibCM config
  // ----------------------------------------------------------------------------
  EmonLibCM_SetADC_VChannel(0, vCal);                      // ADC Input channel, voltage calibration - for Ideal UK Adapter = 268.97
  if (USA) EmonLibCM_SetADC_VChannel(0, vCal_USA);         // ADC Input channel, voltage calibration - for US AC-AC adapter 77DA-10-09
  if (CT1) EmonLibCM_SetADC_IChannel(1, i1Cal, i1Lead);    // ADC Input channel, current calibration, phase calibration
  if (CT2) EmonLibCM_SetADC_IChannel(2, i2Cal, i2Lead);    // The current channels will be read in this order
  if (CT3) EmonLibCM_SetADC_IChannel(3, i3Cal, i3Lead);    // 90.91 for 100 A : 50 mA c.t. with 22R burden - v.t. leads c.t by ~4.2 degrees
  if (CT4) EmonLibCM_SetADC_IChannel(4, i4Cal, i4Lead);    // 16.67 for 100 A : 50 mA c.t. with 120R burden - v.t. leads c.t by ~1 degree

  EmonLibCM_ADCCal(3.3);                                   // ADC Reference voltage, (3.3 V for emonTx,  5.0 V for Arduino)
  // mains frequency 50Hz
  if (USA) EmonLibCM_cycles_per_second(60);                // mains frequency 60Hz
  EmonLibCM_datalog_period(10);                            // period of readings in seconds - normal value for emoncms.org

  EmonLibCM_setPulseEnable(true);                          // Enable pulse counting
  EmonLibCM_setPulsePin(3, 1);
  EmonLibCM_setPulseMinPeriod(0);

  EmonLibCM_setTemperatureDataPin(5);                      // OneWire data pin (emonTx V3.4)
  EmonLibCM_setTemperaturePowerPin(19);                    // Temperature sensor Power Pin - 19 for emonTx V3.4  (-1 = Not used. No sensors, or sensor are permanently powered.)
  EmonLibCM_setTemperatureResolution(11);                  // Resolution in bits, allowed values 9 - 12. 11-bit resolution, reads to 0.125 degC
  EmonLibCM_setTemperatureAddresses(allAddresses);         // Name of array of temperature sensors
  EmonLibCM_setTemperatureArray(allTemps);                 // Name of array to receive temperature measurements
  EmonLibCM_setTemperatureMaxCount(3);                     // Max number of sensors, limited by wiring and array size.
  
  EmonLibCM_TemperatureEnable(true);
  EmonLibCM_Init();                                        // Start continuous monitoring.
  emontx.Msg = 0;
  
}

void loop()
{
  getCalibration();
  
  if (EmonLibCM_Ready())
  {
    #ifdef DEBUG
    if (emontx.Msg==0) {
      Serial.println(EmonLibCM_acPresent()?F("AC present "):F("AC missing "));
      delay(5);
    }
    #endif

    emontx.Msg++;

    // Other options calculated by EmonLibCM
    // RMS Current:    EmonLibCM_getIrms(ch)
    // Apparent Power: EmonLibCM_getApparentPower(ch)
    // Power Factor:   EmonLibCM_getPF(ch)
    
    int CT_index = 0;
    if (CT1) {
      emontx.P1 = EmonLibCM_getRealPower(CT_index);
      emontx.E1 = EmonLibCM_getWattHour(CT_index);
      CT_index++;
    } else {
      emontx.P1 = 0;
      emontx.E1 = 0;
    }

    if (CT2) {
      emontx.P2 = EmonLibCM_getRealPower(CT_index);
      emontx.E2 = EmonLibCM_getWattHour(CT_index);
      CT_index++;
    } else {
      emontx.P2 = 0;
      emontx.E2 = 0;
    }
    
    if (CT3) {
      emontx.P3 = EmonLibCM_getRealPower(CT_index);
      emontx.E3 = EmonLibCM_getWattHour(CT_index);
      CT_index++;
    } else {
      emontx.P3 = 0;
      emontx.E3 = 0;
    }
    
    if (CT4) {
      emontx.P4 = EmonLibCM_getRealPower(CT_index);
      emontx.E4 = EmonLibCM_getWattHour(CT_index);
      CT_index++;
    } else {
      emontx.P4 = 0;
      emontx.E4 = 0;
    }

    emontx.Vrms   = EmonLibCM_getVrms() * 100;
    
    emontx.T1 = allTemps[0];
    emontx.T2 = allTemps[1];
    emontx.T3 = allTemps[2];

    emontx.pulse = EmonLibCM_getPulseCount();
    
    #ifdef ENABLE_RF
      PayloadTX tmp = emontx;
      #ifdef RF_WHITENING
          byte WHITENING = 0x55;
          for (byte i = 0, *p = (byte *)&tmp; i < sizeof tmp; i++, p++)
              *p ^= (byte)WHITENING;
      #endif
      rf12_sendNow(0, &tmp, sizeof tmp);     //send datas
      delay(50);
    #endif
    // ---------------------------------------------------------------------
    // Key:Value format, used by EmonESP & emonhub EmonHubTx3eInterfacer
    // ---------------------------------------------------------------------
    #ifdef PRINT_DATA
      Serial.print(F("MSG:")); Serial.print(emontx.Msg);
      Serial.print(F(",Vrms:")); Serial.print(emontx.Vrms*0.01);
      
      // to show current for calibration:
      #ifdef SHOW_CAL
        if (CT1) { Serial.print(F(",I1:")); Serial.print(EmonLibCM_getIrms(EmonLibCM_getLogicalChannel(1))); }
        if (CT2) { Serial.print(F(",I2:")); Serial.print(EmonLibCM_getIrms(EmonLibCM_getLogicalChannel(2))); }
        if (CT3) { Serial.print(F(",I3:")); Serial.print(EmonLibCM_getIrms(EmonLibCM_getLogicalChannel(3))); }
        if (CT4) { Serial.print(F(",I4:")); Serial.print(EmonLibCM_getIrms(EmonLibCM_getLogicalChannel(4))); }

        if (CT1) { Serial.print(F(",pf1:")); Serial.print(EmonLibCM_getPF(EmonLibCM_getLogicalChannel(1)),4); }
        if (CT2) { Serial.print(F(",pf2:")); Serial.print(EmonLibCM_getPF(EmonLibCM_getLogicalChannel(2)),4); }
        if (CT3) { Serial.print(F(",pf3:")); Serial.print(EmonLibCM_getPF(EmonLibCM_getLogicalChannel(3)),4); }
        if (CT4) { Serial.print(F(",pf4:")); Serial.print(EmonLibCM_getPF(EmonLibCM_getLogicalChannel(4)),4); }
      #endif
  
      if (CT1) { Serial.print(F(",P1:")); Serial.print(emontx.P1); }
      if (CT2) { Serial.print(F(",P2:")); Serial.print(emontx.P2); }
      if (CT3) { Serial.print(F(",P3:")); Serial.print(emontx.P3); }
      if (CT4) { Serial.print(F(",P4:")); Serial.print(emontx.P4); }
      
      if (CT1) { Serial.print(F(",E1:")); Serial.print(emontx.E1); }
      if (CT2) { Serial.print(F(",E2:")); Serial.print(emontx.E2); }
      if (CT3) { Serial.print(F(",E3:")); Serial.print(emontx.E3); }
      if (CT4) { Serial.print(F(",E4:")); Serial.print(emontx.E4); }
      
      if (emontx.T1!=30000) { Serial.print(F(",T1:")); Serial.print(emontx.T1*0.01); }
      if (emontx.T2!=30000) { Serial.print(F(",T2:")); Serial.print(emontx.T2*0.01); }
      if (emontx.T3!=30000) { Serial.print(F(",T3:")); Serial.print(emontx.T3*0.01); }
  
      Serial.print(F(",pulse:")); Serial.println(emontx.pulse);
      delay(20);
    #endif
    // End of print out ----------------------------------------------------
  }
  wdt_reset();
  delay(20);
}
