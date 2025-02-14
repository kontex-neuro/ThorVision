# XDAQ Metadata

```mermaid
---
title: Data Structure of XDAQ Metadata
---
graph LR;
    A(XDAQFrameData);
    A -->|64-bit| B(FPGA Timestamp);
    A -->|32-bit| C(Rhythm Timestamp);
    A -->|32-bit| D(TTL In);
    A -->|32-bit| E(TTL Out);
    A -->|32-bit| F(SPI Performance Counter);
    A -->|64-bit| G(Reserved);
```

XDAQ metadata is embedded into camera frames captured by [XDAQ AIO](https://kontex.io/pages/xdaq) and streamed live to a PC via Thunderbolt. Each recorded frame contains both the JPEG encoded image data and its associated XDAQ metadata.

/// note | Note 
Record H.265 encoded videos (coming soon)
///