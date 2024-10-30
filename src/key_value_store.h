#ifndef KEYVALUESTORE_H
#define KEYVALUESTORE_H

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>


struct KeyValuePair {
    uint64_t key;
    char value[32];

    // Constructor to initialize key and value
    KeyValuePair(uint64_t k, const char *v);
    KeyValuePair();
};

class KeyValueStore
{
public:
    // Constructor
    KeyValueStore(const std::string &filename);

    // Destructor
    ~KeyValueStore();

    // Open the file once before the loop
    void openFile();

    // Close the file once after all operations are done
    void closeFile();

    // Function to append a new key-value pair to the file
    void appendKeyValuePair(const KeyValuePair &kv);

    // Function to read all key-value pairs from the file into memory
    void readAllKeyValuePairs();

    // Function to find the index of a key using binary search
    int findKeyIndex(uint64_t key) const;

    // Getter for key-value pairs
    const std::vector<KeyValuePair> &getKeyValuePairs() const;

    // Function to get the value by index
    const char *getValueByIndex(size_t index) const;

    // Function to get the value by key
    const char *getValueByKey(uint64_t key) const;

private:
    std::string filename;  // Filename for binary file
    std::ofstream outfile;
    std::vector<KeyValuePair> keyValuePairs;  // In-memory key-value pairs
};

#endif  // KEYVALUESTORE_H