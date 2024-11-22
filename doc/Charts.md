# Charts

## Broadcasting

mediasoup **v2** (a room uses a single media worker subprocess by design, so a single CPU).

Charts provided by [CoSMo](https://www.cosmosoftware.io) team.

Scenario:

- 1 peer producing audio and video tracks.
- N spy peers receiving them.

#### Bandwidth out (Mbps) / number of viewers

![](charts/mediasoup_SFU_BW_out.png)

#### Packets per second / number of viewers

![](charts/mediasoup_SFU_packetspersec.png)

#### Average bitrate (bps) and googRTT (ms) / number of viewers

![](charts/mediasoup_clients_getstats.png)

#### CPU usage / number of viewers

![](charts/mediasoup_SFU_cpu.png)
