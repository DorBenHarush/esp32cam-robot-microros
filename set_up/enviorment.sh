#! /bin/bash 


function help {

    echo "Usage: ./enviorment.sh -p <platform_name>"
    echo "Supported platforms:"
    printf "esp32(UART/UDP)\nCrazyflie21(USB/Radio) - congifure before running\n"
    exit
}

#get platform 
while getopts p:h flag
do
    case "${flag}" in
        p) platform=${OPTARG};;
        h) help;;
    esac
done


[ -z $platform ] && help


# Source the ROS 2 installation
source /opt/ros/foxy/setup.bash

# Create a workspace and download the micro-ROS tools
mkdir microros_ws
cd microros_ws
git clone -b $ROS_DISTRO https://github.com/micro-ROS/micro-ros-build.git src/micro-ros-build

# Update dependencies using rosdep
sudo apt update && rosdep update
rosdep install --from-path src --ignore-src -y

# Build micro-ROS tools and source them
colcon build
source install/local_setup.bash


if [ $platform == "esp32cam" ]; then

    #create firmware for esp32cam
    ros2 run micro_ros_setup create_firmware_ws.sh freertos esp32
    pushd firmware/toolchain/esp-idf/components/
    git clone https://github.com/espressif/esp32-camera.git
    popd
else 
    #create firmware for the selected platform 
    ros2 run micro_ros_setup create_firmware_ws.sh freertos $platform
fi

printf "\n\n\nBefore running the next step please connect the board to the computer\nNow you can run the configuration for your app with configure_app.sh\n\n"
echo "Usage:"
echo  "For serial tranportation: ./configure_app.sh -a <app_name> -t serial -d <device_name>"
echo  "device_name can be found using this: ls /dev/serial/by-id/* "
echo  "For UDP transportaion: ./configure_app.sh -a <app_name> -t udp -i <agent_ip> -p <port_number"
echo  "For udp you have to update your WiFi information in the menuconfig step"
echo -e "micro-ROS Transport Settings --> WiFi Configuration\n"








