#!/bin/bash

# Script will exit on the first error
set -e

# Complile/Run the Gateway code
Outputs/Exe/frontGate Configs/frontGateConfig.txt Outputs/Logs/frontGateLog.txt & sleep 1

Outputs/Exe/backGate Configs/backGateConfig.txt Outputs/Logs/backLog.txt & sleep 1


#Run device
Outputs/Exe/device Configs/deviceConfig.txt Outputs/Logs/deviceLog.txt & sleep 1


#Run Sensors
Outputs/Exe/sensor Configs/keyConfig.txt Inputs/keyInput.txt Outputs/Logs/keyOutput.txt & sleep 1

Outputs/Exe/sensor Configs/motionConfig.txt Inputs/motionInput.txt Outputs/Logs/motionOutput.txt & sleep 1

Outputs/Exe/sensor Configs/doorConfig.txt Inputs/doorInput.txt Outputs/Logs/doorOutput.txt & sleep 1 &
