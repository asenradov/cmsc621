#!/bin/bash

gcc -w -pthread frontGate.c -o frontGate
gcc -w -pthread backGate.c -o backGate
gcc -w -pthread sensor.c -o sensor
gcc -w -pthread device.c -o device
