#ifndef PERSON_DETECTOR_HPP
#define PERSON_DETECTOR_HPP
#include <string>
#include <cstdint>
#include <atomic>
#include <thread>
#include <boost/asio.hpp>

#include <brainboard_host.hpp>

typedef struct{
    const int x;
    const int y;
    const int distance;
} Person_t;

class PersonDetector {
public:
    PersonDetector(const std::string& host, const std::string& port, BRAINBOARD_HOST::DeviceController& ref) : host_(host), port_(port), device_controller_(ref) {
    }
    void init();

    Person_t detect_person();

    ~PersonDetector();
    void update_eye_position();

    uint8_t map_value(int value, int from_low, int from_high, int to_low, int to_high);

private:
    std::string host_;
    std::string port_;
    std::atomic<int> _distance{0};
    std::atomic<int> x_position{0};
    std::atomic<int> y_position{0};
    std::atomic_bool person_detected{false};
    std::atomic_bool stop_cond{false};

    std::thread _tcp_recv_thread;
    BRAINBOARD_HOST::DeviceController& device_controller_;
};

#endif /* PERSON_DETECTOR_HPP */