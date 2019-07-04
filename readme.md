# EmonTxV3 Continuous Monitoring Firmware

The following EmonTxV3 continuous monitoring firmware is based on the EmonLibCM library maintained by @Robert.Wall available here: 

- Github: https://github.com/openenergymonitor/EmonLibCM. 
- Original forum post: https://community.openenergymonitor.org/t/emonlibcm-version-2

EmonTxV3CM provides better results than the [EmonTxV3 Discreet Sampling firmware](https://github.com/openenergymonitor/emontx3) which uses the emonLib library, especially where PV Diverters or other fast changing loads are in use. The EmonLibCM library continuously measures in the background the voltage and all the current input channels in turn and calculates a true average quantity for each.

### EmonHub Decoder

To decode the data packet sent by this firmware, add the following emonhub node configuration to /etc/emonhub/emonhub.conf (configurable via the web interface on an EmonBase or EmonPi). 

**Note:** This firmware uses data whitening to improve the reliability of the data transmission. A standard pattern of 1s and 0s is overlayed on the underlying data, this prevents sync issues that result from too many zero values in a packet. When the packet is received it is decoded by emonhub. The 'whitening = 1' is required to tell emonhub to decode the packet correctly.

    [[8]]
      nodename = emontx8
      [[[rx]]]
        names = MSG, Vrms, P1, P2, P3, P4, E1, E2, E3, E4, T1, T2, T3, pulse
        datacodes = L,h,h,h,h,h,L,L,L,L,h,h,h,L
        scales = 1,0.01,1,1,1,1,1,1,1,1,0.01,0.01,0.01,1
        units = n,V,W,W,W,W,Wh,Wh,Wh,Wh,C,C,C,p
        whitening = 1
