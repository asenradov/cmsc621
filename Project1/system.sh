#!/bin/bash

# Script will exit on the first error
set -e

# Complile/Run the Gateway code

./gateway SampleInput/gateConfig.txt > SampleTestRunOutput.txt 2> error.txt &
sleep 5

# Compile/Run the Sensor code
./device SampleInput/senseConfig1.txt SampleInput/senseInput1.txt &
sleep 3
./device SampleInput/senseConfig2.txt SampleInput/senseInput2.txt &
sleep 3
./device SampleInput/senseConfig3.txt SampleInput/senseInput3.txt &
sleep 3
./device SampleInput/senseConfig4.txt SampleInput/senseInput4.txt &
sleep 3

# Compile/Run the Device code
./device SampleInput/devConfig1.txt &
sleep 3
./device SampleInput/devConfig2.txt &

