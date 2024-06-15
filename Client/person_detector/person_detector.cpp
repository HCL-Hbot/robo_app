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
        std::atomic_bool& person_detected, std::atomic_bool& stop_cond, PersonDetector* detector) {
    try {
        boost::asio::io_context io_context;
        boost::asio::ip::tcp::resolver resolver(io_context);
        boost::asio::ip::tcp::resolver::results_type endpoints = resolver.resolve(ip, port);

        std::cout << "port: " << port << " ip: " << ip << '\n';

        // Create and connect the socket
        boost::asio::ip::tcp::socket socket(io_context);
        boost::asio::connect(socket, endpoints);

        // Buffer to store the response
        boost::asio::streambuf buffer;

        std::vector<int> x_values;
        std::vector<int> y_values;
        std::vector<int> distance_values;

        const int max_samples = 10; // Number of samples to average
        const std::chrono::milliseconds update_interval(100); // Update interval

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
                if (parse_json(line, distance_detected, x, y, person_detected)) {
                    x_values.push_back(x);
                    y_values.push_back(y);
                    distance_values.push_back(distance_detected);

                    if (x_values.size() >= max_samples) {
                        // Calculate the average
                        int avg_x = std::accumulate(x_values.begin(), x_values.end(), 0) / max_samples;
                        int avg_y = std::accumulate(y_values.begin(), y_values.end(), 0) / max_samples;
                        int avg_distance = std::accumulate(distance_values.begin(), distance_values.end(), 0) / max_samples;

                        // Update atomic variables
                        x = avg_x;
                        y = avg_y;
                        distance_detected = avg_distance;
                        person_detected = true;

                        // Clear vectors for next batch
                        x_values.clear();
                        y_values.clear();
                        distance_values.clear();

                        // Update eye position
                        detector->update_eye_position();
                    }
                }
            }

            // Throttle the loop to control update frequency
            std::this_thread::sleep_for(update_interval);
        }
    } catch (std::exception &e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }
}

const bool parse_json(const std::string& line, std::atomic<int>& distance_detected, 
    std::atomic<int>& x, std::atomic<int>& y, std::atomic_bool& person_detected) {
    try {
        json parsed = json::parse(line);
        if (parsed.contains("face detected") && parsed.contains("head position")) {
            if (parsed["face detected"].get<int>() == 1) {
                distance_detected = static_cast<int>(parsed["head position"][2].get<double>());
                x = static_cast<int>(parsed["head position"][0].get<double>());
                y = static_cast<int>(parsed["head position"][1].get<double>());
                person_detected = true;
                return true;
            } else {
                person_detected = false;
            }
        }
    } catch (json::parse_error& e) {
        std::cerr << "Parse error: " << e.what() << " - Message: " << line << std::endl;
    }
    return false;
}

const uint8_t PersonDetector::map_value(int value, int from_low, int from_high, int to_low, int to_high) {
    return static_cast<uint8_t>((value - from_low) * (to_high - to_low) / (from_high - from_low) + to_low);
}