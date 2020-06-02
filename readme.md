
# EmonTxV3 Continuous Monitoring Firmware

The following EmonTxV3 continuous monitoring firmware is based on the EmonLibCM library maintained by @Robert.Wall available here: 

- Github: https://github.com/openenergymonitor/EmonLibCM. 
- Original forum post: https://community.openenergymonitor.org/t/emonlibcm-version-2-03/9241

EmonTxV3CM provides better results than the [EmonTxV3 Discrete Sampling firmware](https://github.com/openenergymonitor/emontx3) which uses the emonLib library, especially where PV Diverters or other fast changing loads are in use. The EmonLibCM library continuously measures in the background the voltage and all the current input channels in turn and calculates a true average quantity for each.

## Features

- Continuous Monitoring on all four EmonTxV3 CT channels and ACAC Voltage input channel.
- Real power (Watts) and cumulative energy (Watt-hours) measurement per CT channel.
- RMS Voltage measurement.
- Support for 3x DS18B20 temperature sensors.
- Support for pulse counting.
- Resulting readings transmitted via RFM radio or printed to serial.
- Serial configuration of RFM radio and calibration values.

## How to Compile and upload

If using an RPi, it is assumed the install has been created using the emonScript or an emonSD image. If not, other packages may need to be installed.

### Using PlatformIO

The firmware can be compiled and uploaded on an RPi using [PlatformIO](https://platformio.org). [This may work, but is untested by the author, on other platforms - May 2020].

#### Install PlatformIO

See [PlatformIO install quick start](http://docs.platformio.org/en/latest/installation.html#super-quick-mac-linux)

If installing on an RPi, you may need to run this command

    $ curl -fsSL https://raw.githubusercontent.com/platformio/platformio-core/master/scripts/99-platformio-udev.rules | sudo tee /etc/udev/rules.d/99-platformio-udev.rules

#### Clone this Repository

On newer installations clone the repositoy in the `openenergymonitor` directory.

    $ cd /opt/openenergymonitor/
    $ git clone https://github.com/openenergymonitor/EmonTxV3CM
    $ cd EmonTxV3CM

#### Compile & Upload

All the libs will be automatically installed at the correct versions on first run.

    $ pio run -t upload

If the emonTX is connected to the RPi via the serial interface, The firmware should be complied with

    $ pio run -v -e emontx_pi

If the emonTX is connected to the RPi via the serial interface, the firmware can then be uploaded (ensure emonhub has been stopped - `sudo systemctl stop emonhub.service`) with

    $ avrdude -v -c arduino -p ATMEGA328P -P /dev/ttyAMA0 -b 115200 -U flash:w:output.hex

If not, use a [5v USB to UART cable](https://shop.openenergymonitor.com/programmers) to upload the firmware to emonTx.

Configure EmonHub on the receiving base station to decode the RFM data packet using the decoder below, note data whitening setting or else configure for serial connection.

### Using Arduino IDE

1. Either download directly or use 'git clone' to download this repository into a directory called EmonTxV3CM in your Arduino Sketchbook directory. Ensure that `EmonTxV3CM.ino`, `config.ino` **and** `rfm.ino` are in the same folder.

1. Either download directly or use 'git clone' to download the [EmonLibCM library](https://github.com/openenergymonitor/EmonLibCM) into your Arduino Libraries directory.

1. Open the Arduino IDE (or restart, to reload the libraries). Open the EmonTxV3CM firmware, ensure that `EmonTxV3CM.ino`, `rfm.ino` **and** `config.ino` are present as three tabs in the IDE when the firmware is open.

1. Use a [5v USB to UART cable](https://shop.openenergymonitor.com/programmers) to upload the firmware to emonTx.

1. Configure EmonHub on the receiving base station to decode the RFM data packet using the decoder below, note data whitening setting or else configure for serial connection.

## Serial Configuration

The emonTX can be configured via a serial link.

If the emonTX is connected to the RPi via the serial link, stop emonhub and then use;

    $ miniterm --rtscts /dev/ttyAMA0 115200

Use `+++` then `[Enter]` for config mode.

Available commands for config during start-up:

    b<n>      - set r.f. band n = a single numeral: 4 = 433MHz, 8 = 868MHz, 9 = 915MHz (may require hardware change)
    g<nnn>    - set Network Group  nnn - an integer (OEM default = 210)
    i<nn>     - set node ID i= an integer (standard node ids are 1..30)
    r         - restore sketch defaults
    s         - save config to EEPROM
    v         - show firmware version
    w<x>      - turn RFM Wireless data on or off:
              - x = 0 for OFF, x = 1 for ON, x = 2 for ON with whitening
    x         - exit and continue
    ?         - show this text again

Available commands only when running:

    k<x> <yy.y> <zz.z>
              - Calibrate an analogue input channel:
              - x = a single numeral: 0 = voltage calibration, 1 = ct1 calibration, 2 = ct2 calibration, etc
              - yy.y = a floating point number for the voltage/current calibration constant
              - zz.z = a floating point number for the phase calibration for this c.t. (z is not needed, or ignored if supplied, when x = 0)
              -  e.g. k0 256.8
              -       k1 90.9 2.00
    l         - list the config values
    m<x> <yy> - meter pulse counting:
                x = 0 for OFF, x = 1 for ON, <yy> = an integer for the pulse minimum period in ms. (y is not needed, or ignored when x = 0)
    p<xx.x>   - xx.x = a floating point number for the datalogging period
    s         - save config to EEPROM
    t0 <y>    - turn temperature measurement on or off:
              - y = 0 for OFF, y = 1 for ON
    t<x> <yy> <yy> <yy> <yy> <yy> <yy> <yy> <yy>
              - change a temperature sensor's address or position:
              - x = a single numeral: the position of the sensor in the list (1-based)
              - yy = 8 hexadecimal bytes representing the sensor's address
                e.g.  28 81 43 31 07 00 00 D9
                N.B. Sensors CANNOT be added.
    ?         - show this text again

## EmonHub Decoder

### RFM transmission

To decode the data packet sent via RFM by this firmware, add the following emonhub node configuration to /etc/emonhub/emonhub.conf (configurable via the web interface on an EmonBase or EmonPi).

**This will be added automatically to exsiting emonPi / emonBase after running update.**

#### Data Whitening

This firmware uses data whitening to improve the reliability of the data transmission. A standard pattern of 1s and 0s is overlayed on the underlying data, this prevents sync issues that result from too many zero values in a packet. When the packet is received it is decoded by emonhub.

To setup *whitening*

- If `w2` is set on the emonTx, then `whitening = 1` is needed in the node configuration.
- If `w1` is set on the emonTx, then `whitening = 0` is needed in the node configuration.

#### Node Configuration

    [[15]]
      nodename = emontx3cm15
      [[[rx]]]
        names = MSG, Vrms, P1, P2, P3, P4, E1, E2, E3, E4, T1, T2, T3, pulse
        datacodes = L,h,h,h,h,h,L,L,L,L,h,h,h,L
        scales = 1,0.01,1,1,1,1,1,1,1,1,0.01,0.01,0.01,1
        units = n,V,W,W,W,W,Wh,Wh,Wh,Wh,C,C,C,p
        whitening = 1

    [[16]]
      nodename = emontx3cm16
      [[[rx]]]
        names = MSG, Vrms, P1, P2, P3, P4, E1, E2, E3, E4, T1, T2, T3, pulse
        datacodes = L,h,h,h,h,h,L,L,L,L,h,h,h,L
        scales = 1,0.01,1,1,1,1,1,1,1,1,0.01,0.01,0.01,1
        units = n,V,W,W,W,W,Wh,Wh,Wh,Wh,C,C,C,p
        whitening = 1

### Serial Connection

**Important: To enable printing via serial the RFM69 radio need to be turned off using the serial configuration (`w0`).**

If the RPi is connected via the serial interface directly to the emonTX, then `emonhub.conf` should have this section added to the interfacers section.  The `nodeoffset` will specify the node ID. By default this will be zero. Use an unused NodeID (i.e. a node without a configuration usually 0 - 4)

    [[SerialTx]]
        Type = EmonHubTx3eInterfacer
          [[[init_settings]]]
              com_port= /dev/ttyAMA0
              com_baud = 115200
          [[[runtimesettings]]]
              pubchannels = ToEmonCMS,

              nodeoffset = 1

The data will be sent in `name:value` pairs.

- `MSG` is a count of message number, it increments by one for each message sent. This allows missing data to be identified (but never restored, unfortunately).
- `Vrms` is the voltage in 1/100ths of a Volt.
- `P1 - P4` are the real power value for the 4 c.t. inputs in Watts.
- `E1 - E4` are the accumulated energies for the 4 c.t. inputs in Wh.
- `T1 - T3` are temperatures in 1/100ths of a degree Celsius.
- `pulse` is an accumulating count of pulses as detected by the pulse input.

e.g.

    MSG:1,Vrms:246.81,P1:79,E1:0,pulse:1

The accumulated values start from zero when the sketch starts or is reset. emonCMS is able to handle that reset in most circumstances.

### Further Development

Serial configuration currently supports configuration of the RFM radio and current & voltage channels. Serial configuration of temperature sensors, pulse input and data log period is currently disabled. See commented sections of code. Further work and testing is required to enable these features.

### Licence

This firmware example is open source, licenced under GNU General Public License v3.0.
