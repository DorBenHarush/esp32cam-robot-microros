#! /bin/bash 

#get agruments via flags
while getopts i:p: flag
do
    case "${flag}" in
	i) ip=${OPTARG};;
	p) port=${OPTARG};;
    esac
done

#source the setup
source install/local_setup.bash


#configure the app and start menuconfig
ros2 run micro_ros_setup configure_firmware.sh camera --transport udp --ip $ip --port $port 
ros2 run micro_ros_setup build_firmware.sh menuconfig 

#build and flash the configured firmware
ros2 run micro_ros_setup build_firmware.sh -f

ros2 run micro_ros_setup flash_firmware.sh


#micro-ros agent
ros2 run micro_ros_setup create_agent_ws.sh

ros2 run micro_ros_setup build_agent.sh

source install/local_setup.bash



#running the micro-ros agent
ros2 run micro_ros_agent micro_ros_agent udp4 --port $port

printf "\n\n\n\n Sometimes the flash step fails so Type cd microros_ws && source install/local_setup.bash\nThen try again with: \n ros2 run micro_ros_setup flash_firmware.sh \n and than run the agent again with:\nUDP:\nros2 run micro_ros_agent micro_ros_agent udp4 --port <port_number>\nSerial ros2 run micro_ros_agent micro_ros_agent serial --dev <device_name>\n"

















