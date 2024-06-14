gst-launch-1.0 udpsrc port=5002 caps="application/x-rtp, media=(string)audio, encoding-name=(string)L16, clock-rate=(int)22050, channels=(int)1" buffer-size=500000 ! rtpjitterbuffer latency=100 ! rtpL16depay ! audioconvert ! audioresample ! autoaudiosink &
./build/porcupine_demo_mic

