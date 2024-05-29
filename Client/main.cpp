#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <serial_driver.hpp>

int main() {
    SerialPort serial(PORT_NAME, BAUD_RATE);

    // List available ports
    struct sp_port** ports;
    sp_list_ports(&ports);
    for (int i = 0; ports[i]; i++) {
        std::cout << "Found port: " << sp_get_port_name(ports[i]) << std::endl;
    }
    sp_free_port_list(ports);

    try {
        serial.open();
        std::string data = "Hello, World 2!";
        std::cout << "Sending data: " << data << std::endl;
        serial.write(data);
        std::cout << "Reading data..." << std::endl;
        std::string received_data = serial.read();
        std::cout << "Received data: " << received_data << std::endl;

        serial.close();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}