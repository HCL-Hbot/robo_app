#include <person_detector.hpp>
#include <iostream>
#include <chrono>
#include <iostream>
#include <string>
#include <atomic>
#include <thread>
#include <boost/asio.hpp>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

PersonDetector::~PersonDetector() {
    stop_cond = true;
    _tcp_recv_thread.join();
}

void PersonDetector::init() {
    _tcp_recv_thread = std::thread(person_tracker_thread, host_, port_, 
        std::ref(_distance), std::ref(x_position), std::ref(y_position), 
        std::ref(person_detected), std::ref(stop_cond), this);
}

Person_t PersonDetector::record_person_position() {
    if (person_detected) {
        Person_t new_person_position{x_position, y_position, _distance};
        return new_person_position;
    }
    return {0, 0, 0};
}

void PersonDetector::update_eye_position() {
    int current_target_x = x_position.load();
    int current_target_y = y_position.load();

    if (current_target_x > 430) {
        device_controller_.controlEyes(BRAINBOARD_HOST::EyeID::BOTH, 80, 0, BRAINBOARD_HOST::EyeAnimation::MOVE);
    } else if (current_target_x < 210) {
        device_controller_.controlEyes(BRAINBOARD_HOST::EyeID::BOTH, -80, 0, BRAINBOARD_HOST::EyeAnimation::MOVE);
    } else {
        device_controller_.controlEyes(BRAINBOARD_HOST::EyeID::BOTH, 0, 0, BRAINBOARD_HOST::EyeAnimation::MOVE);
    }
}

void person_tracker_thread(const std::string& ip, const std::string& port, 
        std::atomic<int>& distance_detected, std::atomic<int>& x, std::atomic<int>& y, 
        std::atomic_bool& person_detected, std::atomic_bool& stop_cond, PersonDetector* detector)  {
    try {
        // Create an I/O context
        boost::asio::io_context io_context;

        // Resolve the server address and port
        boost::asio::ip::tcp::resolver resolver(io_context);
        boost::asio::ip::tcp::resolver::results_type endpoints = resolver.resolve(ip, port);

        std::cout << "port: " << port << " ip: " << ip << '\n';

        // Create and connect the socket
        boost::asio::ip::tcp::socket socket(io_context);
        boost::asio::connect(socket, endpoints);

        // Buffer to store the response
        boost::asio::streambuf buffer;

        while (!stop_cond) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            boost::asio::read_until(socket, buffer, '\n');

            // Convert the streambuf into a string
            std::istream is(&buffer);
            std::string line;
            std::getline(is, line);

            // Ensure the buffer is cleared after reading
            buffer.consume(buffer.size());

            // Process the line (either JSON or a simple message)
            if (!line.empty()) {
                try {
                    json parsed = json::parse(line);
                    if (parsed.contains("face detected") && parsed.contains("head position")) {
                        if (parsed["face detected"].get<int>() == 1) {
                            distance_detected = static_cast<int>(parsed["head position"][2].get<double>());
                            x = static_cast<int>(parsed["head position"][0].get<double>());
                            y = static_cast<int>(parsed["head position"][1].get<double>());
                            person_detected = true;
                            detector->update_eye_position(); // Update eye position
                        } else {
                            person_detected = false;
                        }
                    }
                } catch (json::parse_error& e) {
                    std::cerr << "Parse error: " << e.what() << " - Message: " << line << std::endl;
                }
            }
        }
    } catch (std::exception &e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }
}

const uint8_t PersonDetector::map_value(int value, int from_low, int from_high, int to_low, int to_high) {
    return static_cast<uint8_t>((value - from_low) * (to_high - to_low) / (from_high - from_low) + to_low);
}