#ifndef DEVICE_CONTROLLER_HPP
#define DEVICE_CONTROLLER_HPP

#include <sstream>
#include <iomanip>
#include <serial_driver.hpp>
#include <brainboard_refs.hpp>

namespace SERIAL {

class DeviceController {
public:
    DeviceController(const std::string& port_name = PORT_NAME, int baud_rate = BAUD_RATE)
        : serial_port_(port_name, baud_rate) {
        serial_port_.open();
    }

    ~DeviceController() {
        serial_port_.close();
    }

    void controlEye(EyeID eye_nr, uint8_t x, uint8_t y, EyeAnimation anim) {
        std::ostringstream command;
        command << "DS101 " << static_cast<int>(eye_nr) << " " << x << " " << y << " 1000\r\n";
        serial_port_.write(command.str());
        handleEyeAnimation(eye_nr, anim);
    }

    void controlLedstrip(int h, int s, int v, int strip_nr, const std::string& anim) {
        int r, g, b;
        hsv_to_rgb(h, s, v, r, g, b);

        std::ostringstream command;
        command << "LS101 " << strip_nr << " " << r << " " << g << " " << b << "\r\n";
        serial_port_.write(command.str());

        if (anim == "Blink") {
            int duration = 1000;
            command.str(""); // Clear the stream
            command << "LS102 " << strip_nr << " " << r << " " << g << " " << b << " " << duration << "\r\n";
            serial_port_.write(command.str());
        } else if (anim == "Fade In") {
            int duration = 1000;
            command.str("");
            command << "LS104 " << strip_nr << " " << duration << "\r\n";
            serial_port_.write(command.str());
        } else if (anim == "Fade Out") {
            int duration = 1000;
            command.str("");
            command << "LS103 " << strip_nr << " " << duration << "\r\n";
            serial_port_.write(command.str());
        } else if (anim == "Solid Color") {
            // No additional command needed for solid color
        } else {
            std::cerr << "Animation " << anim << " is not implemented" << std::endl;
        }
    }

    void controlHead(MotorID id, int speed, int angle) {
        std::ostringstream command;
        command << "M101 " << static_cast<int>(id) << " " << speed << "\r\n";
        serial_port_.write(command.str());
        command.str("");
        command << "M102 " << static_cast<int>(id) << " " << angle << "\r\n";
        serial_port_.write(command.str());
        command.str("");
    }

private:
    SerialDriver serial_port_;

    void handleEyeAnimation(EyeID eye_nr, EyeAnimation anim) {
        std::ostringstream command;
        switch (anim) {
            case EyeAnimation::BLINK_ANIM:
                command << "DS102 " << static_cast<int>(eye_nr) << " 1\r\n";
                break;
            case EyeAnimation::CONFUSED_ANIM:
                command << "DS103 " << static_cast<int>(eye_nr) << " 1\r\n";
                break;
            case EyeAnimation::THINKING_ANIM:
                command << "DS104 " << static_cast<int>(eye_nr) << " 1\r\n";
                break;
            case EyeAnimation::MOVE:
                std::cerr << "No Animation just move\r\n";
                return; 
            default:
                std::cerr << "Animation not implemented\r\n";
                return;
        }
        serial_port_.write(command.str());
    }

    void hsv_to_rgb(int h, int s, int v, int& r, int& g, int& b) {
        double dh = h / 360.0;
        double ds = s / 100.0;
        double dv = v / 100.0;
        int i = static_cast<int>(dh * 6);
        double f = dh * 6 - i;
        double p = dv * (1 - ds);
        double q = dv * (1 - f * ds);
        double t = dv * (1 - (1 - f) * ds);

        switch (i % 6) {
            case 0: r = dv * 255, g = t * 255, b = p * 255; break;
            case 1: r = q * 255, g = dv * 255, b = p * 255; break;
            case 2: r = p * 255, g = dv * 255, b = t * 255; break;
            case 3: r = p * 255, g = q * 255, b = dv * 255; break;
            case 4: r = t * 255, g = p * 255, b = dv * 255; break;
            case 5: r = dv * 255, g = p * 255, b = q * 255; break;
        }
    }

}; // Class Brainboard_host
} // End of NAMESPACE SERIAL
#endif // DEVICE_CONTROLLER_HPP