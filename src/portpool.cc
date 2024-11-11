#include "portpool.h"

#include <iostream>


PortPool::PortPool(int start_port, int end_port)
{
    base_port = start_port;
    max_port = end_port;
    for (int port = base_port; port < max_port; ++port) {
        available_ports.insert(port);
    }
}

int PortPool::allocate_port()
{
    if (available_ports.empty()) {
        throw std::runtime_error("No available ports");
    }
    int port = *available_ports.begin();
    available_ports.erase(available_ports.begin());
    return port;
}

void PortPool::release_port(int port)
{
    if (base_port <= port && port < max_port) {
        available_ports.insert(port);
    }
}

void PortPool::print_available_ports()
{
    std::cout << "Available Ports: ";
    for (const auto &port : available_ports) {
        std::cout << port << " ";
    }
    std::cout << std::endl;
}
