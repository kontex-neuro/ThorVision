#include "key_value_store.h"

#include <cstring>
#include <iostream>
#include <stdexcept>


// KeyValuePair constructors
KeyValuePair::KeyValuePair(uint64_t k, const char *v) : key(k)
{
    std::memset(value, 0, sizeof(value));
    std::memcpy(value, v, sizeof(value));
}

KeyValuePair::KeyValuePair() : key(0) { std::memset(value, 0, sizeof(value)); }

// KeyValueStore constructor
KeyValueStore::KeyValueStore(const std::string &filename) : filename(filename) {}

KeyValueStore::~KeyValueStore() { closeFile(); }

// Open the file once before the loop
void KeyValueStore::openFile()
{
    outfile.open(filename, std::ios::app | std::ios::binary);
    if (!outfile) {
        throw std::runtime_error("Unable to open file");
    }
}

// Close the file once after all operations are done
void KeyValueStore::closeFile()
{
    if (outfile.is_open()) {
        outfile.close();
    }
}

// Function to append a new key-value pair to the file without reopening
void KeyValueStore::appendKeyValuePair(const KeyValuePair &kv)
{
    if (!outfile.is_open()) {
        throw std::runtime_error("File is not open");
    }

    // Write the key
    outfile.write(reinterpret_cast<const char *>(&kv.key), sizeof(kv.key));
    // Write the value (256 bytes)
    outfile.write(kv.value, sizeof(kv.value));

    // Optional: flush the output to ensure data is written to disk
    // outfile.flush();
}

// Function to read all key-value pairs from the file into memory
void KeyValueStore::readAllKeyValuePairs()
{
    keyValuePairs.clear();  // Clear current vector before reading

    std::ifstream infile(filename, std::ios::binary);
    if (!infile) {
        throw std::runtime_error("Unable to open file");
    }

    // Read until the end of file
    while (!infile.eof()) {
        KeyValuePair kv;
        infile.read(reinterpret_cast<char *>(&kv.key), sizeof(kv.key));
        infile.read(kv.value, sizeof(kv.value));

        if (infile.gcount() == sizeof(kv.value)) {
            keyValuePairs.push_back(kv);  // Only add valid reads
        } else {
            std::cout << "Read error: " << infile.gcount() << std::endl;
        }
    }

    infile.close();
}

// Function to find the index of a key using binary search
int KeyValueStore::findKeyIndex(uint64_t key) const
{
    int left = 0;
    int right = keyValuePairs.size() - 1;

    while (left <= right) {
        int mid = left + (right - left) / 2;

        // Check if the key is present at mid
        if (keyValuePairs[mid].key == key) {
            return mid;
        }

        // If the key is greater, ignore the left half
        if (keyValuePairs[mid].key < key) {
            left = mid + 1;
        }
        // If the key is smaller, ignore the right half
        else {
            right = mid - 1;
        }
    }

    // If we reach here, the key is not present
    return -1;
}

// Getter for key-value pairs
const std::vector<KeyValuePair> &KeyValueStore::getKeyValuePairs() const { return keyValuePairs; }

// Function to get the value by index
const char *KeyValueStore::getValueByIndex(size_t index) const
{
    if (index >= keyValuePairs.size()) {
        throw std::out_of_range("Index out of range");
    }
    return keyValuePairs[index].value;
}

// Function to get the value by key
const char *KeyValueStore::getValueByKey(uint64_t key) const
{
    int index = findKeyIndex(key);
    if (index == -1) {
        throw std::runtime_error("Key not found");
    }
    return keyValuePairs[index].value;
}