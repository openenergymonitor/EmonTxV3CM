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
v1.4: Error checking to EEPROM config
v1.5: Faster RFM factory test
v1.6: Removed reliance on full jeelib for RFM, minimal rfm_send fuction implemented instead, thanks to Robert Wall
v1.7: Check radio channel is clear before transmit
v1.8: PayloadTx.E1 etc were unsigned long. 
v1.9: Unused variables removed.

emonhub.conf node decoder (nodeid is 15 when switch is off, 16 when switch is on)
See: https://github.com/openenergymonitor/emonhub/blob/emon-pi/configuration.md
copy the following in to emonhub.conf:

[[15]]
  nodename = emontx3cm15
  [[[rx]]]
    names = MSG, Vrms, P1, P2, P3, P4, E1, E2, E3, E4, T1, T2, T3, pulse
    datacodes = L,h,h,h,h,h,l,l,l,l,h,h,h,L
    scales = 1,0.01,1,1,1,1,1,1,1,1,0.01,0.01,0.01,1
    units = n,V,W,W,W,W,Wh,Wh,Wh,Wh,C,C,C,p
    whitening = 1

*/
#include <Arduino.h>
#include <avr/wdt.h>

const byte version = 19;                                // Firmware version divide by 10 to get version number e,g 05 = v0.5

// Comment/Uncomment as applicable
#define DEBUG                                           // Debug level print out
// #define SHOW_CAL                                     // Uncomment to show current for calibration

#define RFM69CW
#define RFMSELPIN 10                                    // RFM pins
#define RFMIRQPIN 2                                     // RFM pins
#define RFPWR 0x99                                      // RFM Power setting - see rfm.ino for more

#include "emonLibCM.h"

#include <Wire.h>                                       // Required for RFM & temperature measurement
#include <SPI.h>
#include <util/crc16.h>
#include <OneWire.h>

enum rfband {RF12_433MHZ = 1, RF12_868MHZ, RF12_915MHZ }; // frequency band.

byte RF_freq = RF12_433MHZ;                             // Frequency of radio module can be RF12_433MHZ, RF12_868MHZ or RF12_915MHZ. 
byte nodeID = 15;                                       // node ID for this emonTx.
int networkGroup = 210;                                 // wireless network group, needs to be same as emonBase / emonPi and emonGLCD. OEM default is 210
const int busyThreshold = -97;                          // Signal level below which the radio channel is clear to transmit
const byte busyTimeout = 15;                            // Time in ms to wait for the channel to become clear, before transmitting anyway
int rf_whitening = 2;                                   // RF & data whitening - 0 = no RF, 1 = RF on, no whitening, default = 2: RF is ON with whitening.

typedef struct {
    unsigned long Msg;
    int Vrms,P1,P2,P3,P4; 
    long E1,E2,E3,E4; 
    int T1,T2,T3;
    unsigned long pulse;
} PayloadTX;
PayloadTX emontx;                                       // create an instance
static void showString (PGM_P s);
 
DeviceAddress allAddresses[3];                          // Array to receive temperature sensor addresses
/*   Example - how to define temperature sensors, prevents an automatic search
DeviceAddress allAddresses[] = {       
    {0x28, 0x81, 0x43, 0x31, 0x7, 0x0, 0xFF, 0xD9}, 
    {0x28, 0x8D, 0xA5, 0xC7, 0x5, 0x0, 0x0, 0xD5},      // Use the actual addresses, as many as required
    {0x28, 0xC9, 0x58, 0x32, 0x7, 0x0, 0x0, 0x89}       // up to a maximum of 6    
};
*/

int allTemps[3];                                        // Array to receive temperature measurements

//----------------------------emonTx V3 Settings - Shared with config.ino------------------------
float i1Cal = 90.9;         // (2000 turns / 22 Ohm burden) = 90.9
float i1Lead = 4.2;
float i2Cal = 90.9;         // (2000 turns / 22 Ohm burden) = 90.9
float i2Lead = 4.2;
float i3Cal = 90.9;         // (2000 turns / 22 Ohm burden) = 90.9
float i3Lead = 4.2;
float i4Cal = 16.67;        // (2000 turns / 120 Ohm burden) = 16.67
float i4Lead = 6.0;
float vCal  = 268.97;       // (240V x 13) / 11.6V = 268.97 Calibration for UK AC-AC adapter 77DB-06-09
const float vCal_USA = 130.0;   // Calibration for US AC-AC adapter 77DA-10-09
bool  USA=false;
float period = 9.96;        // datalogging period
bool  pulse_enable = true;  // pulse counting
int   pulse_period = 100;   // pulse min period
bool  temp_enable = true;   // enable temperature measurement
byte  temp_addr[24];        // sensor address data



//----------------------------emonTx V3 hard-wired connections-----------------------------------
const byte LEDpin      = 6;  // emonTx V3 LED
const byte DS18B20_PWR = 19; // DS18B20 Power
const byte DIP_switch1 = 8;  // RF node ID (default no change in node ID, switch on for nodeID + 1) switch off D8 is HIGH from internal pullup
const byte DIP_switch2 = 9;  // Voltage selection 240 / 120 V AC (default switch off 240V)  - switch off D9 is HIGH from internal pullup

//---------------------------------CT availability status----------------------------------------
byte CT_count = 0;
bool CT1, CT2, CT3, CT4;     // Record if CT present during startup

// unsigned long start = 0;

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
 
  load_config(true);                                                        // Load RF config from EEPROM (if any exists)
  
  if (rf_whitening)
  {
    #ifdef DEBUG
      Serial.print(F("RFM69CW only"));
      Serial.print(F(" Node: ")); Serial.print(nodeID);
      Serial.print(" Freq: ");
      if (RF_freq == RF12_433MHZ) Serial.print(F("433MHz"));
      if (RF_freq == RF12_868MHZ) Serial.print(F("868MHz"));
      if (RF_freq == RF12_915MHZ) Serial.print(F("915MHz"));
      Serial.print(F(" Group: ")); Serial.println(networkGroup);
      Serial.println(" ");
    #endif
  }
  
  
  
  // Read status of USA calibration DIP switch----------------------------------------------
  if (digitalRead(DIP_switch2)==LOW) {
      USA=true;                            // IF DIP switch 2 is switched on then activate USA mode
      Serial.print(F("USA Vcal active: ")); Serial.println(vCal_USA);
  }

  // Check connected CT sensors ------------------------------------------------------------
  if (analogRead(1) > 0) {CT1 = 1; CT_count++;} else CT1=0;               // check to see if CT is connected to CT1 input
  if (analogRead(2) > 0) {CT2 = 1; CT_count++;} else CT2=0;               // check to see if CT is connected to CT2 input
  if (analogRead(3) > 0) {CT3 = 1; CT_count++;} else CT3=0;               // check to see if CT is connected to CT3 input
  if (analogRead(4) > 0) {CT4 = 1; CT_count++;} else CT4=0;               // check to see if CT is connected to CT4 input

  // ---------------------------------------------------------------------------------------
  readConfigInput();

  if (rf_whitening)
  {
    rfm_init(RF_freq);                                                           // initialize RFM
    for (int i=10; i>=0; i--)                                             // Send RF test sequence (for factory testing)
    {
      emontx.P1=i;
      PayloadTX tmp = emontx;
      if (rf_whitening == 2)
      {
          byte WHITENING = 0x55;
          for (byte i = 0, *p = (byte *)&tmp; i < sizeof tmp; i++, p++)
              *p ^= (byte)WHITENING;
      }
      rfm_send((byte *)&tmp, sizeof(tmp), networkGroup, nodeID, busyThreshold, busyTimeout);
      delay(100);
    }
    emontx.P1=0;
  }
  
  // ---------------------------------------------------------------------------------------
  
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
  EmonLibCM_SetADC_IChannel(1, i1Cal, i1Lead);             // ADC Input channel, current calibration, phase calibration
  EmonLibCM_SetADC_IChannel(2, i2Cal, i2Lead);             // The current channels will be read in this order
  EmonLibCM_SetADC_IChannel(3, i3Cal, i3Lead);             // 90.91 for 100 A : 50 mA c.t. with 22R burden - v.t. leads c.t by ~4.2 degrees
  EmonLibCM_SetADC_IChannel(4, i4Cal, i4Lead);             // 16.67 for 100 A : 50 mA c.t. with 120R burden - v.t. leads c.t by ~1 degree

  EmonLibCM_ADCCal(3.3);                                   // ADC Reference voltage, (3.3 V for emonTx,  5.0 V for Arduino)
  // mains frequency 50Hz
  if (USA) EmonLibCM_cycles_per_second(60);                // mains frequency 60Hz
  EmonLibCM_datalog_period(period);                        // period of readings in seconds - normal value for emoncms.org  

  EmonLibCM_setPulseEnable(pulse_enable);                  // Enable pulse counting
  EmonLibCM_setPulsePin(3, 1);
  EmonLibCM_setPulseMinPeriod(pulse_period);

  EmonLibCM_setTemperatureDataPin(5);                      // OneWire data pin (emonTx V3.4)
  EmonLibCM_setTemperaturePowerPin(19);                    // Temperature sensor Power Pin - 19 for emonTx V3.4  (-1 = Not used. No sensors, or sensor are permanently powered.)
  EmonLibCM_setTemperatureResolution(11);                  // Resolution in bits, allowed values 9 - 12. 11-bit resolution, reads to 0.125 degC
  EmonLibCM_setTemperatureAddresses(allAddresses);         // Name of array of temperature sensors
  EmonLibCM_setTemperatureArray(allTemps);                 // Name of array to receive temperature measurements
  EmonLibCM_setTemperatureMaxCount(3);                     // Max number of sensors, limited by wiring and array size.
  
  EmonLibCM_TemperatureEnable(temp_enable);  
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
    
    emontx.P1 = EmonLibCM_getRealPower(0); 
    emontx.E1 = EmonLibCM_getWattHour(0); 

    emontx.P2 = EmonLibCM_getRealPower(1); 
    emontx.E2 = EmonLibCM_getWattHour(1); 
    
    emontx.P3 = EmonLibCM_getRealPower(2); 
    emontx.E3 = EmonLibCM_getWattHour(2); 
  
    emontx.P4 = EmonLibCM_getRealPower(3); 
    emontx.E4 = EmonLibCM_getWattHour(3); 

    emontx.Vrms = EmonLibCM_getVrms() * 100;
    
    emontx.T1 = allTemps[0];
    emontx.T2 = allTemps[1];
    emontx.T3 = allTemps[2];

    emontx.pulse = EmonLibCM_getPulseCount();
    
    if (rf_whitening)
    {
      PayloadTX tmp = emontx;
      if (rf_whitening == 2)
      {
          byte WHITENING = 0x55;
          for (byte i = 0, *p = (byte *)&tmp; i < sizeof tmp; i++, p++)
              *p ^= (byte)WHITENING;
      }
      rfm_send((byte *)&tmp, sizeof(tmp), networkGroup, nodeID, busyThreshold, busyTimeout);     //send data
      delay(50);
    }

    // ---------------------------------------------------------------------
    // Key:Value format, used by EmonESP & emonhub EmonHubTx3eInterfacer
    // ---------------------------------------------------------------------
    else
    {
      Serial.print(F("MSG:")); Serial.print(emontx.Msg);
      Serial.print(F(",Vrms:")); Serial.print(emontx.Vrms*0.01);
      
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
    }
    digitalWrite(LEDpin,HIGH); delay(50);digitalWrite(LEDpin,LOW);

    #ifdef SHOW_CAL
      // to show current & power factor for calibration:
  
      Serial.print(F(",I1:")); Serial.print(EmonLibCM_getIrms(EmonLibCM_getLogicalChannel(1)));
      Serial.print(F(",I2:")); Serial.print(EmonLibCM_getIrms(EmonLibCM_getLogicalChannel(2)));
      Serial.print(F(",I3:")); Serial.print(EmonLibCM_getIrms(EmonLibCM_getLogicalChannel(3)));
      Serial.print(F(",I4:")); Serial.print(EmonLibCM_getIrms(EmonLibCM_getLogicalChannel(4)));

      Serial.print(F(",pf1:")); Serial.print(EmonLibCM_getPF(EmonLibCM_getLogicalChannel(1)),4);
      Serial.print(F(",pf2:")); Serial.print(EmonLibCM_getPF(EmonLibCM_getLogicalChannel(2)),4);
      Serial.print(F(",pf3:")); Serial.print(EmonLibCM_getPF(EmonLibCM_getLogicalChannel(3)),4);
      Serial.print(F(",pf4:")); Serial.print(EmonLibCM_getPF(EmonLibCM_getLogicalChannel(4)),4);
    #endif
    // End of print out ----------------------------------------------------
  }
  wdt_reset();
  delay(20);
}
