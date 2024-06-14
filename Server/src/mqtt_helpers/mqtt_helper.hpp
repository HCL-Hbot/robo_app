#ifndef MQTT_HELPER_HPP
#define MQTT_HELPER_HPP
#include <string>
#include <iostream>
#include <mosquitto.h>
#include <settings.hpp>


/* Messages that might be published on the MQTT server */
const std::string mqtt_up_msg = "Server is ready!";
const std::string mqtt_listening_msg = "Server is listening!";
const std::string audio_timeout_msg = "Timeout!";

/**
 * @brief Helper function to quickly initialize a client connection to mqtt server (with error checking)
 * 
 * @return mosquitto* Pointer to dynamically allocated instance of mosquitto mqtt client
 */
mosquitto *mqtt_init()
{
    mosquitto_lib_init();
    mosquitto *mosq = mosquitto_new(nullptr, true, nullptr);
    if (!mosq)
    {
        std::cerr << "Failed to create Mosquitto instance.\n";
        /* When we can't allocate, we don't want to proceed anyway. So exit(1) it is :) */
        exit(1);
    }
    return mosq;
}

/**
 * @brief Connect to an MQTT broker.
 * 
 * This function connects to an MQTT broker using the provided Mosquitto instance.
 * 
 * @param mosq Pointer to the Mosquitto instance.
 * @param host_ip IP address of the MQTT broker.
 * @param host_port Port of the MQTT broker.
 */
void mqtt_connect(mosquitto *mosq, const std::string &host_ip, const uint32_t host_port)
{
    int keepalive = 60;
    if (mosquitto_connect(mosq, host_ip.c_str(), host_port, keepalive) != MOSQ_ERR_SUCCESS)
    {
        std::cerr << "Failed to connect to broker.\n";
        /* Exit when unable to connect, because proceeding would be useless anyway! */
        exit(1);
    }
}

/**
 * @brief Send a message to an MQTT topic.
 * 
 * This function publishes a message to a specified MQTT topic using the provided Mosquitto instance.
 * 
 * @param mosq Pointer to the Mosquitto instance.
 * @param topic The MQTT topic to publish to.
 * @param msg The message to publish.
 * @return int 0 on success, 1 on failure.
 */
int mqtt_send_msg(mosquitto *mosq, const std::string &topic, const std::string &msg)
{
    if (mosquitto_publish(mosq, nullptr, topic.c_str(), msg.length(), msg.c_str(), 0, false) != MOSQ_ERR_SUCCESS)
    {
        std::cerr << "Failed to publish message.\n";
        return 1;
    }
    return 0;
}


/**
 * @brief Clean up the MQTT connection and library.
 * 
 * This function disconnects from the MQTT broker, destroys the Mosquitto instance, 
 * and cleans up the Mosquitto library.
 * 
 * @param mosq Pointer to the Mosquitto instance.
 */
void clean_up_mqtt(mosquitto *mosq) {
    mosquitto_disconnect(mosq);
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
}


#endif /* MQTT_HELPER_HPP */