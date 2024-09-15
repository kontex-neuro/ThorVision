#pragma once

#include <algorithm>
#include <iostream>
#include <unordered_set>

class PortPool
{
public:
    PortPool(int start_port, int end_port);
    int allocate_port();
    void release_port(int port);
    void print_available_ports();

private:
    std::unordered_set<int> available_ports;
    int base_port;
    int max_port;
};
