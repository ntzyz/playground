Pcap port statistics
====================

Collect all traffic data by port.

### Requirements

libpcap

### Build

```
g++ -std=c++1z -o port_stat main.cpp -lpthread `pcap-config --libs` 
```