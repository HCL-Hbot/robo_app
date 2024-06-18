# Healthbot - Robo Application
This is a dual side repository, with the Client and Server-side codebases. Please keep in mind that the Client runs on the Raspberry Pi 4 (Healthbot's Brain) and the Server on the HCL-Edge server.

![Healthbot-Robo-Image](/docs/RoboReleased-V1-Example.png)

## Capabilities
This architecture forms the basis of a system capable of performing complex tasks such as automated control, sensor data processing, and user interaction, making it suitable for advanced robotics or automated interactive systems.

# Highlevel Design
For convenience the whole system's architecture is included in this Robo Application repository.
The presented system architecture illustrates an integrated solution that combines Cloud-Edge and embedded system technologies for real-time social interactive applications.
![Healthbot Tech Architecture](/docs/v3.0%20-%20Architecture%20Healthbot.jpg)

- ***Central Unit (Brain)***: The heart of the system is the Raspberry Pi 4 (RPi4), running on Raspbian OS. It hosts a client application, an OpenOCD Debug Server, a Terminal for manual control, and a Gaze Tracker. This central unit communicates with the HCL-Edge Server, which handles MQTT messages and audio machine learning processing.
- ***Secure Communication***: The brainboard is connected via a secure Tailscale network and RTP streaming, enhancing data transfer integrity.
Embedded Controller (Nervous System): The RPi PICO, running on FreeRTOS, acts as an embedded controller managing various drivers (stepper motor, LED, radar, and display) through structured task command interpretations and driver command processing.
- ****Hardware Framework****: The framework supports numerous devices, including radar sensors, LEDs, displays, and audio components. Coordination is achieved via UART and MQTT protocols, ensuring robust and flexible operations.

# Features
The application controls the entire system via the Raspberry Pi Client (Healthbot's Brain), which manages the Raspberry Pi Pico (Healthbot's Nervous System). The architecture highlights this integration and the connections to the hardware.

- **Robot Main System**: The RPI4 (Brain) handles on-robot processing, including the Client program.
- **Robot Control System**: The Pico (Nervous System) is an embedded system responsible for executing drivers.
- **Interactive Conversation**: Utilizing the latest Machine Learning audio models, the robot can engage in conversations with users.
- **Smooth & Silent Head Movement**: Ensures seamless and quiet head movements.
- **Interactive Eyes**: Equipped with a gaze tracking system, the robot can maintain eye contact and display various animated effects, such as blinking and expressions of thinking or confusion.
- **mmWave Radar Sensor**: Detects movement and, in the future, will be able to record heartbeat and breathing patterns.
- **Lighted Body & Head**: Features an 8-bit RGB color-scheme for lighting up the body and head.
- **Cute Cheeks LEDs**: Added to enhance friendly and open interaction with users.

# System Dependencies
It is quite a list of what you need to install to be able to make the app operational. Here is the total overview: 

## External Libraries
Here are the most important libraries, but it is not guaranteed to be complete. The best way to figure out what is missing in this list is to install a fresh OS image and to document this.  

- Boost
- Mosquitto
- GStreamer

## Internal Libraries / Subsystems
- Gaze Estimator: used for user's position tracking so that the app can utilize eye tracking and head movement. 

## Roadmap Vision
This application is specifically (but quickly and sometimes dirty) developed for the Healthbot-Robo. And the app should be redesigned (but in the basis copied) for each new robot. In the future also the Brainboard should be able to configure the Raspberry Pi Pico (Healtbot's Nervous System) with the detailed parameters of what is present the robot and could be controlled through the nervous system. 