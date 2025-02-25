# Metadata Processing
 
This guide illustrates how to process and synchronize metadata from XDAQ binary files containing video frame information with neural recordings from Intan RHD files. It explains the file format and data structure, lists the software requirements, and provides detailed Python code examples for reading XDAQ metadata into a structured NumPy array, loading Intan neural data, and mapping neural timestamps to video frame timestamps using binary search.

## File Extension

XDAQ data is saved as a binary (**.bin**) file that contains **timestamps** of video frames and associated **metadata**.

## Data Structure

For a full understanding of the structure of the XDAQ metadata, please refer to the [metadata](xdaq-metadata.md) page.

Each data point is 40 bytes with the following layout:

| Field           | Size (bytes) | Description                                 |
|-----------------|--------------|---------------------------------------------|
| video_timestamp | 8            | Timestamp of the video frame                |
| metadata        | 32           | Metadata associated with the video frame including **fpga_timestamp**, **rhythm_timestamp**, **ttl_in**, **ttl_out**, **spi_perf_counter**, and **reserved**  |



The metadata structure is defined as follows:

| Field             | Size (bytes) | Description             |
|-------------------|--------------|-------------------------|
| fpga_timestamp    | 8            | ThorVision timestamp    |
| rhythm_timestamp  | 4            | Rhythm timestamp        |
| ttl_in            | 4            | TTL in                  |
| ttl_out           | 4            | TTL out                 |
| spi_perf_counter  | 4            | SPI performance counter |
| reserved          | 8            | Reserved                |

## Requirements

Ensure you have the following installed for compatibility:

- **Python 3.6+** (for type hints support)
- **NumPy >= 1.15.0** (for required array operations)
- **Intan RHD/RHS reader** (for neural data, download from [Intan Technologies](https://intantech.com/downloads.html?tabSelect=Software))

*Note:* Although earlier versions might work, these versions are recommended.

## Reading XDAQ Metadata

The following example shows how to read XDAQ metadata from a binary file into a structured NumPy array:

```python
import sys
from typing import Optional
import numpy as np

# Define data types for better code organization
metadata_dtype = np.dtype([
    ('fpga_timestamp', np.uint64),
    ('rhythm_timestamp', np.uint32),
    ('ttl_in', np.uint32),
    ('ttl_out', np.uint32),
    ('spi_perf_counter', np.uint32),
    ('reserved', np.uint64)
])

record_dtype = np.dtype([
    ('video_timestamp', np.uint64),
    ('metadata', metadata_dtype)
])

def read_XDAQFrameData_np(filename: str) -> Optional[np.ndarray]:
    """
    Read XDAQ metadata data file.
    
    Args:
        filename: Path to the metadata data file.
        
    Returns:
        Structured numpy array containing video timestamps and metadata.
        Returns None if file reading fails.
        
    Example:
        >>> data = read_XDAQFrameData_np("data.bin")
        >>> print(data['video_timestamp'][0])  # First video timestamp
        >>> print(data['metadata']['ttl_in'][0])  # First TTL input value
    """
    try:
        with open(filename, 'rb') as f:
            data = f.read()
        return np.frombuffer(data, dtype=record_dtype)
    except Exception as e:
        print(f"Error reading file: {e}")
        return None
```

**Expected output:**

```
array([(1234567890, (987654321, 1000, 1, 0, 100, 0)),
       (1234567891, (987654322, 1001, 1, 0, 101, 0)), ...],
      dtype=record_dtype)
```

## Reading Intan Data

The next code snippet uses the Intan Python reader to load data from an .rhd file. You can download the reader from [Intan Technologies](https://intantech.com/downloads.html?tabSelect=Software).

```python
import sys
from load_intan_rhd_format import read_data

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(f"Usage: python {sys.argv[0]} <file_name>")
        sys.exit(1)
    rhd_path = sys.argv[1]
    intan_data = read_data(rhd_path)
    print(f"Loaded neural data with {len(intan_data['t_amplifier'])} samples") 
    print(intan_data['t_amplifier'])
    print(intan_data['amplifier_data'])
```

**Usage:**

```bash
python XDAQFrameData.py <intan_data_file_name>
```

**Expected output:**

```
Reading Intan Technologies RHD Data File, Version 3.3

Found 128 amplifier channels.
Found 6 auxiliary input channels.
Found 0 supply voltage channels.
Found 0 board ADC channels.
Found 8 board digital input channels.
Found 0 board digital output channels.
Found 0 temperature sensors channels.

File contains 28.915 seconds of data.  Amplifiers were sampled at 30.00 kS/s.

Allocating memory for data...
Reading data from file...
10% done...
20% done...
30% done...
40% done...
50% done...
60% done...
70% done...
80% done...
90% done...
Parsing data...
No missing timestamps in data.
Done!  Elapsed time: 1.2 seconds
Loaded neural data with 867456 samples
[0.00000000e+00 3.33333333e-05 6.66666667e-05 ... 2.89151000e+01
 2.89151333e+01 2.89151667e+01]
[[ 435.63   562.38   618.345 ...  533.91   531.57   527.865]
 [ 475.8    592.995  629.07  ...  633.75   629.85   624.39 ]
 [ 881.595 1027.26  1087.125 ... 1037.01  1029.795 1020.63 ]
 ...
 [  67.665   56.355   43.68  ...  300.495  300.3    292.695]
 [ 181.35   301.665  264.42  ...  187.2    186.225  180.57 ]
 [ 160.68   283.14   240.825 ...  375.375  373.815  362.505]]
```

## Synchronizing XDAQ and Intan Data

This example demonstrates how to map neural timestamps to the corresponding video frame timestamps using a binary search with NumPy.

```python
import numpy as np
import sys
from load_intan_rhd_format import read_data

# Define a structured dtype for the metadata
metadata_dtype = np.dtype([
    ('fpga_timestamp', np.uint64),
    ('rhythm_timestamp', np.uint32),
    ('ttl_in', np.uint32),
    ('ttl_out', np.uint32),
    ('spi_perf_counter', np.uint32),
    ('reserved', np.uint64)
])

record_dtype = np.dtype([
    ('video_timestamp', np.uint64),
    ('metadata', metadata_dtype)
])

def read_XDAQFrameData_np(filename: str) -> np.ndarray:
    with open(filename, 'rb') as f:
        data = f.read()
    return np.frombuffer(data, dtype=record_dtype)

def map_rhd_to_video(neural_timestamps: np.ndarray, video_timestamps: np.ndarray) -> np.ndarray:
    """
    For each neural timestamp, find the video frame index for which the video timestamp
    is the largest value less than or equal to the neural timestamp.
    """
    indices = np.searchsorted(video_timestamps, neural_timestamps, side='right') - 1
    indices[indices < 0] = 0
    return indices

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(f"Usage: python {sys.argv[0]} <rhd_file_name> <xdaq_file_name>")
        sys.exit(1)
        
    rhd_data = read_data(sys.argv[1])
    xdaq_data = read_XDAQFrameData_np(sys.argv[2])
    
    rhd_timestamps = rhd_data["t_amplifier"]
    video_timestamps = xdaq_data["video_timestamp"]
    
    mapped_indices = map_rhd_to_video(rhd_timestamps, video_timestamps)
    target_index_rhd = 0
    target_index_video = mapped_indices[target_index_rhd]
    print(f"index_rhd: {target_index_rhd}, index_video: {target_index_video}")
    print(f"video_timestamp: {video_timestamps[target_index_video]}, rhd_timestamp: {rhd_timestamps[target_index_rhd]}")
    print(f"video_metadata: {xdaq_data['metadata'][target_index_video]}")
```

**Usage:**

```bash
python XDAQFrameData.py <intan_data_file_name> <xdaq_data_file_name>
```

**Expected output:**

```
Reading Intan Technologies RHD Data File, Version 3.3

Found 128 amplifier channels.
Found 6 auxiliary input channels.
Found 0 supply voltage channels.
Found 0 board ADC channels.
Found 8 board digital input channels.
Found 0 board digital output channels.
Found 0 temperature sensors channels.

File contains 28.915 seconds of data.  Amplifiers were sampled at 30.00 kS/s.

Allocating memory for data...
Reading data from file...
10% done...
20% done...
30% done...
40% done...
50% done...
60% done...
70% done...
80% done...
90% done...
Parsing data...
No missing timestamps in data.
Done!  Elapsed time: 1.1 seconds
index_rhd: 0, index_video: 0
video_timestamp: 26213000000, rhd_timestamp: 0.0
video_metadata: (78118894449, 0, 0, 0, 809483121, 0)
```
