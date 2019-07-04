/*

EEPROM layout

Byte
0     NodeID
1     RF Freq
2     Network Group
3-6   vCal
7-10  i1Cal
11-14 i1Lead
15-18 i2Cal
19-22 i2Lead
23-26 i3Cal
27-30 i3Lead
31-34 i4Cal
35-38 i4Lead
39-42 Datalogging period
43    Temperatures enabled
44-68 Temperature Addresses
69    Pulses enabled
70-71 Pulse min period


*/
#include <avr/pgmspace.h>
#include <EEPROM.h>

 // Available Serial Commands
const PROGMEM char helpText1[] =                                
"\n"
"Available commands for config during start-up:\n"
"  <nnn>g    - set Network Group\n"
"  <nn>i     - set node ID (standard node ids are 1..30)\n"
"  r         - restore sketch defaults\n"
"  s         - save config to EEPROM\n"
"  v         - Show firmware version\n"
"  x         - exit and continue\n"
"\n"
"Available commands when running:\n"
"  k<x> <yy.y> <zz.z>\n"
"            - Calibrate an analogue input channel:\n"
"            - x = a single numeral: 0 = voltage calibration, 1 = ct1 calibration, 2 = ct2 calibration, etc\n"
"            - yy.y = a floating point number for the voltage/current calibration constant\n"
"            - zz.z = a floating point number for the phase calibration for this c.t. (z is not needed, or ignored if supplied, when x = 0)\n"
"            - e.g. k0 256.8\n"
"            -      k1 90.9 2.00\n"
"  l         - list the config values\n"
//"  m<x> <yy> - meter pulse counting:\n"
//"              x = 0 for OFF, x = 1 for ON, <yy> = an integer for the pulse minimum period in ms. (y is not needed, or ignored when x = 0)\n"
//"  p <xx.x>  - xx.x = a floating point number for the datalogging period" 
"  s         - save config to EEPROM\n"
//"  t0 <y>    - turn temperature measurement on or off:\n"
//"            - y = 0 for OFF, y = 1 for ON\n"
//"  t<x> <yy> <yy> <yy> <yy> <yy> <yy> <yy> <yy>\n"
//"            - set a temperature sensor:\n"
//"            - x = a single numeral: the position of the sensor in the list (1-based)\n"
//"            - yy = 8 hexadecimal bytes representing the sensor's address\n"
//"              e.g.  28 81 43 31 07 00 00 D9 \n"

;

struct eeprom {byte nodeID; byte RF_freq; byte networkGroup; float vCal, i1Cal, i1Lead, i2Cal, i2Lead, i3Cal, i3Lead, i4Cal, i4Lead;} data;

byte value;
extern DeviceAddress *temperatureSensors;

static void load_config(bool verbose)
{
  byte* src = (byte *)&data;
  bool dataPresent = (EEPROM.read(0) != 255);
 
  if (dataPresent)
  {
    for (byte j=0; j<sizeof(data); j++, src++)
          *src = EEPROM.read(j); 

    nodeID       = data.nodeID;
    // RF_freq      = data.RF_freq;
    networkGroup = data.networkGroup;
    vCal         = data.vCal;
    i1Cal        = data.i1Cal;
    i1Lead       = data.i1Lead;
    i2Cal        = data.i2Cal;
    i2Lead       = data.i2Lead;
    i3Cal        = data.i3Cal;
    i3Lead       = data.i3Lead; 
    i4Cal        = data.i4Cal; 
    i4Lead       = data.i4Lead;
  }    
  
  if (verbose)
  {
    if (dataPresent) {
      Serial.println(F("Loaded EEPROM config"));
    } else { 
      Serial.println(F("No EEPROM config"));
    }
    list_calibration();
  }    
}

static void list_calibration(void)
{
  Serial.println(F("Calibration:"));
  Serial.print(F("vCal = ")); Serial.println(vCal);
  Serial.print(F("i1Cal = ")); Serial.println(i1Cal);
  Serial.print(F("i1Lead = ")); Serial.println(i1Lead);
  Serial.print(F("i2Cal = ")); Serial.println(i2Cal);
  Serial.print(F("i2Lead = ")); Serial.println(i2Lead);
  Serial.print(F("i3Cal = ")); Serial.println(i3Cal);
  Serial.print(F("i3Lead = ")); Serial.println(i3Lead);
  Serial.print(F("i4Cal = ")); Serial.println(i4Cal);
  Serial.print(F("i4Lead = ")); Serial.println(i4Lead);
  
  printTemperatureSensorAddresses();
}

static void save_config()
{
  Serial.println("Saving...");

  //Save new settings
  byte* src = (byte*) &data;
  data.nodeID       = nodeID;  
  // data.RF_freq   = RF_freq;
  data.networkGroup = networkGroup;
  data.vCal         = vCal;
  data.i1Cal        = i1Cal;
  data.i1Lead       = i1Lead;
  data.i2Cal        = i2Cal;
  data.i2Lead       = i2Lead;
  data.i3Cal        = i3Cal;
  data.i3Lead       = i3Lead; 
  data.i4Cal        = i4Cal; 
  data.i4Lead       = i4Lead;      
  
  for (byte j=0; j<sizeof(data); j++, src++)
    EEPROM[j] = *src;    

  for (byte j=0; j<sizeof(data); j++)
    Serial.print(EEPROM[j]);Serial.print(" ");
  
  Serial.println(F("Done. New config saved to EEPROM"));
}

static void wipe_eeprom(void)
{
  byte* src = (byte*)&data;
  Serial.println(F("Resetting..."));
  
  for (byte j=0; j<sizeof(data); j++)
    EEPROM[j] = 255;
    
  Serial.println(F("Done. Sketch will now restart using default config."));
  delay(200);
}

void softReset(void)
{
  asm volatile ("  jmp 0");
}

void readInput(void)
{
  Serial.println(F("POST.....wait 10s"));
  Serial.println(F("'+++' then [Enter] for config mode"));
    
  unsigned long start = millis();
  bool done = false;
  while (millis() < (start + 10000))
  {
    // If serial input of keyword string '+++' is entered during 10s POST then enter config mode
    if (Serial.available())
    {
      if ( Serial.readString() == "+++\r\n")
      {
        Serial.println(F("Entering config mode..."));
        showString(helpText1);
        // char c[]="v"
        done = config(char('v'));
        while(!done)
        {
          if (Serial.available())
          {
            done = config(Serial.read());
          }
        }
      }
    }
  }
}



static bool config(char c) 
{
  
  if ('0' <= c && c <= '9') 
  {
    value = 10 * value + c - '0';
    return false;
  }

  if (c > ' ') 
  {
    switch (c) 
    {
      case 'i': //set node ID

        if (value)
        {
          nodeID = value;
        }
        Serial.print(F("[Node ")); Serial.print(nodeID & 0x1F); Serial.print(F("]"));
        break;
/*
      case 'b': // set band: 4 = 433, 8 = 868, 9 = 915
        value = bandToFreq(value);
        if (value){
          RF_FREQ = value;
        }
        break;
*/
      case 'g': // set network group
        if (value) // Group 0 is not valid when transmitting
          networkGroup = value;
        Serial.print(F("[Group ")); Serial.print(networkGroup); Serial.print(F("]"));
        break;

      case 'l': // print the calibration values
		    list_calibration();
        break;

      case 'r': // restore sketch defaults
        wipe_eeprom();
        softReset();
        break;

      case 's': // Save to EEPROM. Atemga328p has 1kB  EEPROM
        save_config();
        break;

      case 'v': // print firmware version
        Serial.print(F("emonTx V3.4 EmonLibCM Continuous Monitoring V")); Serial.println(version*0.1);
        break;
      
      case 'x':  // exit and continue
        return true;
        
      
      default:
        showString(helpText1);
    } //end switch
    
    /*
    //Print Current RF config
    if (RF_STATUS==1)
    {
      Serial.print(F(" "));
      Serial.print((char) ('@' + (nodeID & 0x1F)));
      Serial.print(F(" i"));
      Serial.print(nodeID & 0x1F);

      Serial.print(F(" g"));
      Serial.print(networkGroup);

      Serial.print(F(" @ "));
      Serial.print(RF_freq == RF12_433MHZ ? 433 :
                   RF_freq == RF12_868MHZ ? 868 :
                   RF_freq == RF12_915MHZ ? 915 : 0);
      Serial.print(F(" MHz"));
    }
    */
    
    Serial.println(F(" "));
  } 
  value = 0;
  return false;

}



void getCalibration(void)
{
/*
 * Reads calibration information (if available) from the serial port. Data is expected in the format
 * 
 *  k[x] [y] [z]
 * 
 * where:
 *  [x] = a single numeral: 0 = voltage calibration, 1 = ct1 calibration, 2 = ct2 calibration, etc
 *  [y] = a floating point number for the voltage/current calibration constant
 *  [z] = a floating point number for the phase calibration for this c.t. (z is not needed, or ignored if supplied, when x = 0)
 * 
 * e.g. k0 256.8
 *      k1 90.9 1.7 
 * 
 * If power factor is not displayed, it is impossible to calibrate for phase errors,
 *  and the standard value of phase calibration MUST BE SENT when a current calibration is changed.
 * 
 */

	if (Serial.available())
  {
    int k1; 
    double k2, k3; 
    char c = Serial.peek();
    switch (c) {
      case 'f':
        /*
        *  Format expected: f50 | f60
        */
        k1 = Serial.parseFloat();
        while (Serial.available())
          Serial.read(); 
        EmonLibCM_cycles_per_second(k1);
        break;
      
      case 'k':
        /*  Format expected: k[x] [y] [z]
        * 
        * where:
        *  [x] = a single numeral: 0 = voltage calibration, 1 = ct1 calibration, 2 = ct2 calibration, etc
        *  [y] = a floating point number for the voltage/current calibration constant
        *  [z] = a floating point number for the phase calibration for this c.t. (z is not needed, or ignored if supplied, when x = 0)
        * 
        * e.g. k0 256.8
        *      k1 90.9 1.7 
        * 
        * If power factor is not displayed, it is impossible to calibrate for phase errors,
        *  and the standard value of phase calibration MUST BE SENT when a current calibration is changed.
        */
        k1 = Serial.parseFloat(); 
        k2 = Serial.parseFloat(); 
        k3 = Serial.parseFloat(); 
        while (Serial.available())
          Serial.read(); 
              
        // Write the values back as Globals, re-calculate intermediate values.
        switch (k1) {
          case 0 : EmonLibCM_ReCalibrate_VChannel(k2);
            vCal = k2;
            break;
              
          case 1 : EmonLibCM_ReCalibrate_IChannel(1, k2, k3);
            i1Cal = k2;
            i1Lead = k3;
            break;

          case 2 : EmonLibCM_ReCalibrate_IChannel(2, k2, k3);
            i2Cal = k2;
            i2Lead = k3;
            break;

          case 3 : EmonLibCM_ReCalibrate_IChannel(3, k2, k3);
            i3Cal = k2;
            i3Lead = k3;
            break;

          case 4 : EmonLibCM_ReCalibrate_IChannel(4, k2, k3);
            i4Cal = k2;
            i4Lead = k3;
            break;
                       
          default : ;
        }
        break;
          
      case 'l':
        list_calibration(); // print the calibration values
      break;

      
      // case 'm' :
        /*  Format expected: m[x] [y]
         * 
         * where:
         *  [x] = a single numeral: 0 = pulses OFF, 1 = pulses ON,
         *  [y] = an integer for the pulse min period in ms - ignored when x=0
         */
        /*
        k1 = Serial.parseFloat(); 
        k2 = Serial.parseFloat(); 
        while (Serial.available())
          Serial.read(); 
              
        switch (k1) {
          case 0 : EmonLibCM_setPulseEnable(false);
            break;
          
          case 1 : EmonLibCM_setPulseMinPeriod(k2);
            EmonLibCM_setPulseEnable(true);
            break;
        }
        break;        
        */
        
      // case 'p':
        /*  Format expected: p[x]
         * 
         * where:
         *  [x] = a floating point number for the datalogging period in s
         */
        /*
        k2 = Serial.parseFloat(); 
        while (Serial.available())
          Serial.read();
        EmonLibCM_datalog_period(k2); 
        // save for EEPROM if required
        break;
        */
        
      // case 't' :
        /*  Format expected: t[x] [y] [y] ...
         */
        // set_temperatures();
        // break;

      case 's' :
        save_config(); // Save to EEPROM. ATMega328p has 1kB  EEPROM
        break;
    }
    // flush the input buffer
    while (Serial.available())
      Serial.read();
  }
}

/*
static byte bandToFreq (byte band) {
  return band == 4 ? RF12_433MHZ : band == 8 ? RF12_868MHZ : band == 9 ? RF12_915MHZ : 0;
}
*/

static void showString (PGM_P s) {
  for (;;) {
    char c = pgm_read_byte(s++);
    if (c == 0)
      break;
    if (c == '\n')
      Serial.print('\r');
    Serial.print(c);
  }
}

void set_temperatures(void)
{
  /*  Format expected: t[x] [y] [y] ...
  * 
  * where:
  *  [x] = 0  [y] = single numeral: 0 = temperature measurement OFF, 1 = temperature measurement ON or OFF
  *  [x] = a single numeral > 0: the position of the sensor in the list (1-based)
  *  [y] = 8 hexadecimal bytes representing the sensor's address
  *          e.g. t2 28 81 43 31 07 00 00 D9
  */
    
  DeviceAddress sensorAddress;
         
	int k1 = Serial.parseFloat();

  if (k1 == 0)
  { 
    EmonLibCM_TemperatureEnable(Serial.parseInt());
    // write to EEPROM
  }
  else if (k1 > EmonLibCM_getTemperatureSensorCount())
    return;
  else
  {
    byte i = 0, a, b;
    Serial.readBytes(&b,1);     // expect a leading space
    while (Serial.readBytes(&b,1) && i < 8)
    {            
      if (b == ' ' || b == '\r' || b == '\n')
      {
        sensorAddress[i++] = a;
        a = 0;
      }                
      else
      {
        a *= 16;
        a += c2h(b);
      }          
    }     
    // set address
    for (byte i=0; i<8; i++)
      allAddresses[k1-1][i] = sensorAddress[i];
  }
	while (Serial.available())
		Serial.read(); 
}

byte c2h(byte b)
{
  if (b > 47 && b < 58) 
    return b - 48;
  else if (b > 64 && b < 71) 
    return b - 55;
  else if (b > 96 && b < 103) 
    return b - 87;
  return 0;
}
