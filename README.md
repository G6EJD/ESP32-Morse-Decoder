# ESP32-Morse-Decoder
Using an ESP32 and OLED with a basic microphone to decode Morse Code live to the display

You can use the ESP8266, but will need to expriement with the sampling_frequency, try 30000

If you need to reduce the audio bandwidth of the decoder; currently about 320Hz to cope with very noisy bands/audio, then adjust these lines:
float sampling_freq = 45000;
float target_freq   = 558.0; // adjust for your needs see above
int   n = 128;               // if you change here please change next line also

n = 128 determines the number of samples, try to get this as high as possible to reduce bandwidth, I went for a compromise with 128.

Sampling frequency is a function of CPU speed, so largely fixed, but can be veried downwards to about 30000, needs some experimentation with your set-up.

Target frequency is the received audio frequency, if you prefer a higher beat for Morse tones, then adjust accordingly and remeber to tune your radio to try to match your chosen frequency.

