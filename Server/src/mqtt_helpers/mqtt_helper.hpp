#ifndef MQTT_HELPER_HPP
#define MQTT_HELPER_HPP
#include <string>
#include <iostream>
#include <mosquitto.h>
#include <settings.hpp>

const std::string mqtt_up_msg = "Server is ready!";
const std::string mqtt_listening_msg = "Server is listening!";
const std::string audio_timeout_msg = "Timeout!";

mosquitto *mqtt_init()
{
    mosquitto_lib_init();
    mosquitto *mosq = mosquitto_new(nullptr, true, nullptr);
    if (!mosq)
    {
        std::cerr << "Failed to create Mosquitto instance.\n";
        exit(1);
    }
    return mosq;
}

void mqtt_connect(mosquitto *mosq, const std::string &host_ip, const uint32_t host_port)
{
    int keepalive = 60;
    if (mosquitto_connect(mosq, host_ip.c_str(), host_port, keepalive) != MOSQ_ERR_SUCCESS)
    {
        std::cerr << "Failed to connect to broker.\n";
        exit(1);
    }
}

int mqtt_send_msg(mosquitto *mosq, const std::string &topic, const std::string &msg)
{
    if (mosquitto_publish(mosq, nullptr, topic.c_str(), msg.length(), msg.c_str(), 0, false) != MOSQ_ERR_SUCCESS)
    {
        std::cerr << "Failed to publish message.\n";
        return 1;
    }
    return 0;
}


void clean_up_mqtt(mosquitto *mosq) {
    mosquitto_disconnect(mosq);
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
}
#endif /* MQTT_HELPER_HPP */