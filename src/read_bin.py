import struct
import os
from dataclasses import dataclass
from typing import List, Tuple

@dataclass
class XDAQFrameData:
    fpga_timestamp: int
    rhythm_timestamp: int
    ttl_in: int
    ttl_out: int
    spi_perf_counter: int
    reserved: int

@dataclass
class KeyValuePair:
    key: int
    value: bytes

class KeyValueStore:
    def __init__(self, filename: str):
        self.filename = filename
        self.key_value_pairs: List[KeyValuePair] = []

    def append_key_value_pair(self, kv: KeyValuePair):
        with open(self.filename, 'ab') as f:
            f.write(struct.pack('Q', kv.key))
            f.write(kv.value.ljust(32, b'\0'))

    def read_all_key_value_pairs(self):
        self.key_value_pairs.clear()
        if not os.path.exists(self.filename):
            raise FileNotFoundError("Unable to open file")

        with open(self.filename, 'rb') as f:
            while True:
                key_data = f.read(8)
                if not key_data:
                    break
                key = struct.unpack('Q', key_data)[0]
                value = f.read(32)
                if len(value) == 32:
                    self.key_value_pairs.append(KeyValuePair(key, value))
                else:
                    print(f"Read error: {len(value)} bytes read")

    def find_key_index(self, key: int) -> int:
        left, right = 0, len(self.key_value_pairs) - 1
        while left <= right:
            mid = (left + right) // 2
            if self.key_value_pairs[mid].key == key:
                return mid
            elif self.key_value_pairs[mid].key < key:
                left = mid + 1
            else:
                right = mid - 1
        return -1

    def get_key_value_pairs(self) -> List[KeyValuePair]:
        return self.key_value_pairs

    def get_value_by_index(self, index: int) -> bytes:
        if index >= len(self.key_value_pairs):
            raise IndexError("Index out of range")
        return self.key_value_pairs[index].value

    def get_value_by_key(self, key: int) -> bytes:
        index = self.find_key_index(key)
        if index == -1:
            raise KeyError("Key not found")
        return self.key_value_pairs[index].value

def main(filename: str, search_key: int):
    store = KeyValueStore(filename)
    store.read_all_key_value_pairs()

    key_value_pairs = store.get_key_value_pairs()
    for index, kv in enumerate(key_value_pairs):
        frame_data = struct.unpack('QIIIIQ', kv.value[:32])
        # rhythm_timestamp = struct.unpack('f', struct.pack('I', frame_data[1]))[0]
        print(f"Index: {index}, Key: {kv.key}, frame_data->fpga_timestamp: {frame_data[0]}, frame_data->rhythm_timestamp: {frame_data[1]}")


    print(f"Size: {len(key_value_pairs)}")

    # index = store.find_key_index(search_key)
    # print(f"Key: {search_key} Index: {index}")
    # if index != -1:
    #     frame_data = struct.unpack('QIIIIQ', store.get_value_by_key(search_key)[:32])
    #     print(f"Value: {frame_data[0]} getValueByKey {search_key}")
    # else:
    #     print("Key not found")

    # if 0 <= index < len(key_value_pairs):
    #     frame_data = struct.unpack('QIIIIQ', store.get_value_by_index(index)[:32])
    #     print(f"Value: {frame_data[0]} getValueByIndex {index}")
    # else:
    #     print("Index out of range")

if __name__ == "__main__":
    import sys
    if len(sys.argv) != 3:
        print("Usage: python read_bin.py <filename> <search_key>")
        sys.exit(1)
    filename = sys.argv[1]
    search_key = int(sys.argv[2])
    main(filename, search_key)