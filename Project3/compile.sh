#!/bin/bash

gcc -w -pthread src/Gateway/frontGate.c -o Outputs/Exe/frontGate
gcc -w -pthread src/Database/backGate.c -o Outputs/Exe/backGate
gcc -w -pthread src/Sensor/sensor.c -o Outputs/Exe/sensor
gcc -w -pthread src/SecuritySystem/device.c -o Outputs/Exe/device
