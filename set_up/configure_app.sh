#! /bin/bash 


#help function
function help {

	echo "Usage:"
	echo  "For serial tranportation: ./configure_app.sh -a <app_name> -t serial -d <device_name>"
	echo  "device_name can be found using this: ls /dev/serial/by-id/* "
	echo  "For UDP transportaion: ./configure_app.sh -a <app_name> -t udp -i <agent_ip> -p <port_number"
	echo  "For udp you have to update your WiFi information in the menuconfig step"
        echo  -e "micro-ROS Transport Settings --> WiFi Configuration\n"
        exit

}


#get agruments via flags
while getopts a:t:d:i:p: flag
do
    case "${flag}" in
        a) app=${OPTARG};;
        t) transport=${OPTARG};;
        d) device=${OPTARG};;
	i) ip=${OPTARG};;
	p) port=${OPTARG};;
    esac
done

cd microros_ws

#source the setup
source install/local_setup.bash


#checks the argumnets
if [ "$transport" == "udp" ]; then
    [ -z $ip ] && help
    [ -z $port ] && help
    ros2 run micro_ros_setup configure_firmware.sh $app --transport $transport --ip $ip --port $port 
    ros2 run micro_ros_setup build_firmware.sh menuconfig 

elif [ "$transport" == "serial" ]; then
    [ -z $device ] && help
    ros2 run micro_ros_setup configure_firmware.sh $app --transport $transport
else
    help
fi

#build and flash the configured firmware
ros2 run micro_ros_setup build_firmware.sh

ros2 run micro_ros_setup flash_firmware.sh


#micro-ros agent
ros2 run micro_ros_setup create_agent_ws.sh

ros2 run micro_ros_setup build_agent.sh

source install/local_setup.bash



#running the micro-ros agent
if [ "$transport" == "udp" ]; then

    ros2 run micro_ros_agent micro_ros_agent udp4 --port $port
else
    ros2 run micro_ros_agent micro_ros_agent serial --dev $device
fi

printf "\n\n\n\n Sometimes the flash step fails so Type cd microros_ws && source install/local_setup.bash\nThen try again with: \n ros2 run micro_ros_setup flash_firmware.sh \n and than run the agent again with:\nUDP:\nros2 run micro_ros_agent micro_ros_agent udp4 --port <port_number>\nSerial ros2 run micro_ros_agent micro_ros_agent serial --dev <device_name>\n"

















