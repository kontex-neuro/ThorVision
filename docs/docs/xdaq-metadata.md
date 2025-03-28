# XDAQ Metadata

## Data Structure

```mermaid
graph LR;
    A(XDAQFrameData);
    A -->|8 bytes| B(FPGA Timestamp);
    A -->|4 bytes| C(Rhythm Timestamp);
    A -->|4 bytes| D(TTL In);
    A -->|4 bytes| E(TTL Out);
    A -->|4 bytes| F(SPI Performance Counter);
    A -->|8 bytes| G(Reserved);
```

`XDAQFrameData` is embedded into camera frames captured by [XDAQ AIO](https://www.kontex.io/xdaq) and streamed live to a PC via Thunderbolt. Each recorded frame contains both the JPEG encoded image data and its associated XDAQ metadata.

/// note | Note
Record H.265 encoded videos (coming soon)
///

```mermaid
graph LR;
    A(PtsMetadata);
    A -->|8 bytes| B(PTS);
    A -->|32 bytes| C(XDAQFrameData);
```

## File Extension

`PtsMetadata` is saved as a flat binary file, containing continuous metadata associated with video frames for further [metadata processing](metadata-processing.md).
