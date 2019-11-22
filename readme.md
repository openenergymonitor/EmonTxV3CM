
# EmonTxV3 Continuous Monitoring Firmware

The following EmonTxV3 continuous monitoring firmware is based on the EmonLibCM library maintained by @Robert.Wall available here: 

- Github: https://github.com/openenergymonitor/EmonLibCM. 
- Original forum post: https://community.openenergymonitor.org/t/emonlibcm-version-2

EmonTxV3CM provides better results than the [EmonTxV3 Discreet Sampling firmware](https://github.com/openenergymonitor/emontx3) which uses the emonLib library, especially where PV Diverters or other fast changing loads are in use. The EmonLibCM library continuously measures in the background the voltage and all the current input channels in turn and calculates a true average quantity for each.

### Features

- Continuous Monitoring on all four EmonTxV3 CT channels and ACAC Voltage input channel.
- Real power (Watts) and cumulative energy (Watt-hours) measurement per CT channel.
- RMS Voltage measurement.
- Support for 3x DS18B20 temperature sensors.
- Support for pulse counting.
- Resulting readings transmitted via RFM radio and printed to serial.
- Serial configuration of RFM radio and calibration values.

### How to Compile 

#### Using PlatformIO

Compile and upload firmware using [PlatformIO](https://platformio.org)

##### Install platformio (if needed)

See [platformio install quick start](http://docs.platformio.org/en/latest/installation.html#super-quick-mac-linux)

Recommended to use install script which may require sudo:

`python -c "$(curl -fsSL https://raw.githubusercontent.com/platformio/platformio/master/scripts/get-platformio.py)"`


##### Clone this repo 

    $ git clone https://github.com/openenergymonitor/EmonTxV3CM
    $ cd EmonTxV3CM

##### Upload & Upload

    $ pio run -t upload

All the libs will be automatically installed at the correct versions. 

##### Using Arduino IDE

1. Either download directly or use 'git clone' to download this repository into a directory called EmonTxV3CM in your Arduino Sketchbook directory. Ensure that both EmonTxV3CM.ino **and** config.ino are in the same folder.

2. Either download directly or use 'git clone' to download the [EmonLibCM library](https://github.com/openenergymonitor/EmonLibCM) into your Arduino Libraries directory.

3. Either download directly or use 'git clone' to download the [JeeLib library](https://github.com/jcw/jeelib) into your Arduino Libraries directory. 

4. Open the Arduino IDE (or restart, to reload the libraries). Open the EmonTxV3CM firmware, ensure that both EmonTxV3CM.ino **and** config.ino are present as two tabs in the IDE when the firmware is open.

5. Use a [5v USB to UART cable](https://shop.openenergymonitor.com/programmers) to upload the firmware to emonTx.

6. Configure EmonHub on the receiving base station to decode the RFM data packet using the decoder below, note data whitening setting.

**Todo:** PlatformIO compilation and upload instructions.

#### EmonHub Decoder

To decode the data packet sent by this firmware, add the following emonhub node configuration to /etc/emonhub/emonhub.conf (configurable via the web interface on an EmonBase or EmonPi).

**This will be added automatically to exsiting emonPi / emonBase after running update.** 

**Note:** This firmware uses data whitening to improve the reliability of the data transmission. A standard pattern of 1s and 0s is overlayed on the underlying data, this prevents sync issues that result from too many zero values in a packet. When the packet is received it is decoded by emonhub. The 'whitening = 1' is required to tell emonhub to decode the packet correctly.

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

### Further Development

Serial configuration currently supports configuration of the RFM radio and current & voltage channels. Serial configuration of temperature sensors, pulse input and data log period is currently disabled. See commented sections of code. Further work and testing is required to enable these features.

### Licence

This firmware example is open source, licenced under GNU General Public License v3.0.
