# 8bit-sd-pwm

Arduino based audio player with control codes embedded in audio stream.

## Purpose

To play back recorded audio and control other I/O, such as LEDs, at precise points in the audio stream.

The resulting audio has significant "tape hiss" but is otherwise clear and high quality.

Other solutions can provide better quality audio with less hassle, but they may not offer tight synchronization with the audio stream. For instance one board I tried had a high, variable latency between the start command and actually starting.

The synchronization here is rounded to 16ms frames (1/62 second) - compare to SMPTE time code at typically 30 frames/second.

## General Concept
  * Prepare an audio track
  * Prepare a label file of events in the audio track - can be done in Audacity
    * Each event is a code from 1-255
  * Merge audio and labels into a binary file and place on a MicroSD card
  * Modify the event handler function on the Arduino to react to your event codes
  * When the audio is playing, the event handler function gets called at the right time for each event
  

## Characteristics:
  * 8 bit
  * 32 kHz sample rate (16kHz max bandwidth)
  * ~62 control frames/second
    * Implies +- 8ms quantization error for events - well under perceptual threshold
  * One byte of control data per frame
  * Raw SD card without filesystem

## Hardware Requirements

  * Arduino UNO
  * Adafruit MicroSD shield
  * MicroSD adapter for PC - for recording audio
  * Simple low-pass filter
  * Amplifier/speaker

## Electrical Connections

```
Arduino MicroSD
----------------
10      CS
11      DI
12      DO
13      CLK
GND     GND
5V      5V

Arduino External
-----------------
3       Audio out to low-pass filter
A0      Start button
A1      Stop button
```

## Steps


Prepare an audio track.
In Audacity, add labels at key points in the track. The labels should have names like "a3" which means opcode 3. Export the labels to a file.

Convert the audio to mono 8-bit unsigned:
```
sox /mag/ytp/mirlitons.wav -c1 -r32000 mirlitons.ub
```

Merge the audio and labels:
```
python2 mkbin.py something.ub labels.txt out.bin
```

Write the binary file to the MicroSD card. Warning - make sure you get the device right.
Make sure the card is not mounted - you are writing to the raw device. Any filesystem on the card will be destroyed.
```
sudo dd if=out.bin of=/dev/sdd bs=512
```

Connect the card to the Arduino, program the sketch, and open the serial monitor. Ground A0 briefly to start the audio. Yo should get audio at pin3, and your opcodes should show up in the serial monitor.



