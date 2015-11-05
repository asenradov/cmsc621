#!/bin/bash

gcc -w -pthread gateway.c -o gateway
gcc -w -pthread device.c -o device

