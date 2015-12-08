gnome-terminal -e "bash -c \"../Outputs/Exe/frontGate ../Configs/frontGateConfig.txt GatewayOutput.log; exec bash\"" 
sleep 1
gnome-terminal -e "bash -c \"../Outputs/Exe/backGate ../Configs/backGateConfig.txt PersistentStorage.txt; exec bash\"" 
sleep 1
gnome-terminal -e "bash -c \"../Outputs/Exe/sensor ../Configs/doorConfig.txt ../Inputs/doorInput.txt DoorOutput.log; exec bash\"" 
sleep 1
gnome-terminal -e "bash -c \"../Outputs/Exe/sensor ../Configs/keyConfig.txt  ../Inputs/keyInput.txt KeychainOutput.log; exec bash\"" 
sleep 1
gnome-terminal -e "bash -c \"../Outputs/Exe/sensor ../Configs/motionConfig.txt ../Inputs/motionInput.txt MotionOutput.log; exec bash\"" 
sleep 1
gnome-terminal -e "bash -c \"../Outputs/Exe/device ../Configs/deviceConfig.txt SecurityDeviceOutput.log; exec bash\""
