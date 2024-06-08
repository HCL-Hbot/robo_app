#include <chrono>
#include <thread>
#include <iostream>
#include <brainboard_host.hpp>

int main() {
    BRAINBOARD_HOST::DeviceController controller("/dev/ttyS0", 115200);

    //controller.controlEye(BRAINBOARD_HOST::EyeID::LEFT, 100, 100, BRAINBOARD_HOST::EyeAnimation::MOVE);
    // controller.controlEye(BRAINBOARD_HOST::EyeID::RIGHT, 100, 100, BRAINBOARD_HOST::EyeAnimation::BLINK_ANIM, 2000);
    controller.controlHeadMovement(BRAINBOARD_HOST::MotorID::AZIMUTH, 50, 100);
    controller.controlHeadMovement(BRAINBOARD_HOST::MotorID::ELEVATION, 10, 100);
    controller.setColor(BRAINBOARD_HOST::LedID::BODY, BRAINBOARD_HOST::Color::BLUE);
    return 0;
}