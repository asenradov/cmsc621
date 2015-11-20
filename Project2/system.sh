#!/bin/bash

# Script will exit on the first error
set -e

# Complile/Run the Gateway code
./frontGate Configs/frontGateConfig.txt Outputs/frontGateLog.txt & sleep 1

./backGate Configs/backGateConfig.txt Outputs/backLog.txt &sleep 1


#Run device
./device Configs/deviceConfig.txt Outputs/deviceLog.txt &sleep 1


#Run Sensors
./sensor Configs/keyConfig.txt Inputs/keyInput.txt Outputs/keyOutput.txt & sleep 1

./sensor Configs/motionConfig.txt Inputs/motionInput.txt Outputs/motionOutput.txt & sleep 1

./sensor Configs/doorConfig.txt Inputs/doorInput.txt Outputs/doorOutput.txt & sleep 1 &
