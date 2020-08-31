# esp32cam-microros
The following document describes how to create firmware, configure it with a microros program and flash it to an esp32camera board.
Based on https://micro-ros.github.io/docs/tutorials/core/first_application_rtos/freertos/.

# step 1: Ros 2 enviorment
Micro-ros is Ros-2-Foxy based. The process can be done in from:
* Install ROS 2 Foxy FitzRoy on your Ubuntu 20.04 LTS computer from [Ros 2](https://index.ros.org/doc/ros2/Installation/Foxy/Linux-Install-Debians/).
* Docker container with ROS 2 Foxy FitzRoy.

Serial: 
docker run -it --net=host -v /dev:/dev --privileged  ros:foxy .

UDP: 
docker run -it --net=host -v /dev:/dev --privileged -p 8888:8888/udp  ros:foxy (the port can be any port).

relocate the file enviormnet.sh to the root rirectory of the docker like this:
`docker cp envoirmnet.sh <docker_name>://` 
and then run `chmod +x envoirment.sh` (same for configure_app.sh)

Usage: 
`./enviorment.sh -p esp32cam`
* -p can be any freertos platform supported by [Micro-ros](https://github.com/micro-ROS/micro_ros_setup)

# step 2: configure application

Serial:
`./configure.sh -a <app> -t serial -d <device_name>`                                                                                                                 
UDP:
`./configure_app.sh -a <app> -t udp -i <local_ip> -p <port_number>`                                                                                               
Costumized apps should be added to `firmware/freertos_apps/apps/<my_app>`
* `<my_app>/app.c`: This file contains the program.
* `<my_app>/app-colcon.meta`: This file contains the micro-ROS app specific colcon configuration. Detailed info on how to configure the RMW via this file can be found [here](https://micro-ros.github.io/docs/tutorials/core/microxrcedds_rmw_configuration/).
* Header files (".h").

