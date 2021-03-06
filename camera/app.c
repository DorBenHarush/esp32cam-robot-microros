#include <esp_event_loop.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include <string.h>

#include <stdio.h>
#include <unistd.h>

#include <rcl/rcl.h>
#include <rcl/error_handling.h>
#include <sensor_msgs/msg/compressed_image.h>


#include <rclc/rclc.h>
#include <rclc/executor.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/mcpwm.h"
#include "soc/mcpwm_periph.h"

#include "boards.h"

#define STRING_CAPACITY 20
#define GPIO_PWM0A_OUT 15   //Set GPIO 15 as PWM0A
#define GPIO_PWM0B_OUT 16   //Set GPIO 16 as PWM0B

#define GPIO_PWM1A_OUT 13   //Set GPIO 13 as PWM1A
#define GPIO_PWM1B_OUT 14   //Set GPIO 1 as PWM1B

static const char *TAG = "stream_pictures";

#define RCCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){printf("Failed status on line %d: %d. Aborting.\n",__LINE__,(int)temp_rc);vTaskDelete(NULL);}}
#define RCSOFTCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){printf("Failed status on line %d: %d. Continuing.\n",__LINE__,(int)temp_rc);}}

//ROS and camera variables

rcl_publisher_t publisher;
sensor_msgs__msg__CompressedImage* msg;
camera_fb_t *pic;

//MCPWM config
mcpwm_config_t pwm_0_config;
mcpwm_config_t pwm_1_config;

static void mcpwm_gpio_initialize(void)
{
    printf("Initializing gpio. \n");
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, GPIO_PWM0A_OUT);
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0B, GPIO_PWM0B_OUT);
    mcpwm_gpio_init(MCPWM_UNIT_1, MCPWM1A, GPIO_PWM1A_OUT);
    mcpwm_gpio_init(MCPWM_UNIT_1, MCPWM1B, GPIO_PWM1B_OUT);
}



static void brushed_motor_forward(mcpwm_unit_t mcpwm_num, mcpwm_timer_t timer_num , float duty_cycle)
{
    mcpwm_set_signal_low(mcpwm_num, timer_num, MCPWM_OPR_B);
    mcpwm_set_duty(mcpwm_num, timer_num, MCPWM_OPR_A, duty_cycle);
    mcpwm_set_duty_type(mcpwm_num, timer_num, MCPWM_OPR_A, MCPWM_DUTY_MODE_0); //call this each time, if operator was previously in low/high state
}


static void brushed_motor_backward(mcpwm_unit_t mcpwm_num, mcpwm_timer_t timer_num , float duty_cycle)
{
    mcpwm_set_signal_low(mcpwm_num, timer_num, MCPWM_OPR_A);
    mcpwm_set_duty(mcpwm_num, timer_num, MCPWM_OPR_B, duty_cycle);
    mcpwm_set_duty_type(mcpwm_num, timer_num, MCPWM_OPR_B, MCPWM_DUTY_MODE_0);  //call this each time, if operator was previously in low/high state
}


static void brushed_motor_stop(mcpwm_unit_t mcpwm_num, mcpwm_timer_t timer_num)
{
    mcpwm_set_signal_low(mcpwm_num, timer_num, MCPWM_OPR_A);
    mcpwm_set_signal_low(mcpwm_num, timer_num, MCPWM_OPR_B);
}


static void initialize_parameters_of_mcpwm(mcpwm_config_t* pwm_config, bool unit)
{
	printf("Initializing prameters of MCPWM. \n");	
	pwm_config->frequency = 1000;    //frequency = 500Hz,
	pwm_config->cmpr_a = 0;    //duty cycle of PWMxA = 0
	pwm_config->cmpr_b = 0;    //duty cycle of PWMxb = 0
	pwm_config->counter_mode = MCPWM_UP_COUNTER;
	pwm_config->duty_mode = MCPWM_DUTY_MODE_0;
	(unit == 0) ? mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, pwm_config) : mcpwm_init(MCPWM_UNIT_1, MCPWM_TIMER_1, pwm_config); //Configure PWM0A & PWM0B with above settings
}



static camera_config_t camera_config = {
    .pin_pwdn = CAM_PIN_PWDN,
    .pin_reset = CAM_PIN_RESET,
    .pin_xclk = CAM_PIN_XCLK,
    .pin_sscb_sda = CAM_PIN_SIOD,
    .pin_sscb_scl = CAM_PIN_SIOC,

    .pin_d7 = CAM_PIN_D7,
    .pin_d6 = CAM_PIN_D6,
    .pin_d5 = CAM_PIN_D5,
    .pin_d4 = CAM_PIN_D4,
    .pin_d3 = CAM_PIN_D3,
    .pin_d2 = CAM_PIN_D2,
    .pin_d1 = CAM_PIN_D1,
    .pin_d0 = CAM_PIN_D0,
    .pin_vsync = CAM_PIN_VSYNC,
    .pin_href = CAM_PIN_HREF,
    .pin_pclk = CAM_PIN_PCLK,

    //XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental)
    .xclk_freq_hz = 20000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,

    .pixel_format = PIXFORMAT_JPEG, //YUV422,GRAYSCALE,RGB565,JPEG
    .frame_size = FRAMESIZE_HVGA,    //QQVGA-UXGA Do not use sizes above QVGA when not JPEG

    .jpeg_quality = 12, //0-63 lower number means higher quality
    .fb_count = 1       //if more than one, i2s runs in continuous mode. Use only with JPEG
};

void timer_callback(rcl_timer_t * timer, int64_t last_call_time)
{
	UNUSED(last_call_time);
	if (timer != NULL) {
		
	}
}

static esp_err_t init_camera()
{
    //initialize the camera
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Camera Init Failed.");
		
        return err;
    }

    return ESP_OK;
}



void moveMotors()
{
	//init mcpwm for motors
	mcpwm_gpio_initialize();
	initialize_parameters_of_mcpwm(&pwm_0_config, 0);
	initialize_parameters_of_mcpwm(&pwm_1_config, 1);
	//activate motors
	brushed_motor_forward(MCPWM_UNIT_0, MCPWM_TIMER_0, 50.0);
	brushed_motor_forward(MCPWM_UNIT_1, MCPWM_TIMER_1, 50.0);
	while(1);
	//stop motors
	brushed_motor_stop(MCPWM_UNIT_0, MCPWM_TIMER_0);
	brushed_motor_stop(MCPWM_UNIT_1, MCPWM_TIMER_1);
	vTaskDelete(NULL);

}



void appMain(void * arg)
{
	//init camera	
	init_camera();

	

	rcl_allocator_t allocator = rcl_get_default_allocator();
	rclc_support_t support;
	msg = sensor_msgs__msg__CompressedImage__create();

	// create init_options
	RCCHECK(rclc_support_init(&support, 0, NULL, &allocator));

	// create node
	rcl_node_t node = rcl_get_zero_initialized_node();
	RCCHECK(rclc_node_init_default(&node, "freertos_picture_publisher", "", &support));

	// create publisher
	RCCHECK(rclc_publisher_init_default(&publisher, &node, ROSIDL_GET_MSG_TYPE_SUPPORT(sensor_msgs, msg, CompressedImage), "freertos_picture_publisher"));

	// create timer,
	rcl_timer_t timer = rcl_get_zero_initialized_timer();
	const unsigned int timer_timeout = 1000;
	RCCHECK(rclc_timer_init_default(&timer, &support, RCL_MS_TO_NS(timer_timeout), timer_callback));

	// create executor
	rclc_executor_t executor = rclc_executor_get_zero_initialized_executor();
	RCCHECK(rclc_executor_init(&executor, &support.context, 1, &allocator));

	unsigned int rcl_wait_timeout = 1000;   // in ms
	RCCHECK(rclc_executor_set_timeout(&executor, RCL_MS_TO_NS(rcl_wait_timeout)));
	RCCHECK(rclc_executor_add_timer(&executor, &timer));

	
	//set pixel format and frame id
	char * encoding = "jpeg";
	msg->format.data = (char*)heap_caps_malloc(strlen(encoding) + 1, MALLOC_CAP_8BIT); 
	msg->format.capacity = strlen(encoding) + 1;
	memset(msg->format.data, 0, strlen(encoding) + 1);
	memcpy(msg->format.data, encoding, strlen(encoding));
	msg->format.size = strlen(encoding) + 1;

	char* frame_id = "AI_thinker_image";
	msg->header.frame_id.data = (char*)heap_caps_malloc(strlen(frame_id) + 1, MALLOC_CAP_8BIT);
	msg->header.frame_id.capacity = strlen(encoding) + 1;
	memset(msg->header.frame_id.data, 0, strlen(frame_id) + 1);
	memcpy(msg->header.frame_id.data, frame_id, strlen(frame_id));
	msg->header.frame_id.size = strlen(frame_id) + 1;

	//activate motors
	brushed_motor_forward(MCPWM_UNIT_0, MCPWM_TIMER_0, 50.0);
	brushed_motor_forward(MCPWM_UNIT_1, MCPWM_TIMER_1, 50.0);


	while(1)
	{
		rclc_executor_spin_some(&executor, 1000);
		usleep(100000);
		
		pic = esp_camera_fb_get();
		//allocate memory for data
		msg->data.data = (uint8_t*)heap_caps_realloc(msg->data.data, pic->len, MALLOC_CAP_8BIT);
		msg->data.capacity = pic->len;
		msg->data.size = pic->len;
		//check for NULL ptr
		if(msg->data.data == NULL)
		{
			printf("Failed to allocate memory. restating in 5 sec.\n");
			vTaskDelay(5000 / portTICK_RATE_MS);
			esp_restart();
		}
		printf("Successfully allocated %d bytes. copying...\n", pic->len);
		//copy data
		memcpy(msg->data.data, pic->buf, pic->len);
		//print msg info
		printf("Pixel format %s \n", msg->format.data);
		printf("Frame id %s \n", msg->header.frame_id.data);
		printf("Buffer length %d \n", msg->data.size);
		//publish picture
		printf("Publisher return value: %d \n", rcl_publish(&publisher, msg, NULL));
		vTaskDelay(200 / portTICK_RATE_MS);

	}


	// free resources
	RCCHECK(rcl_publisher_fini(&publisher, &node))
	RCCHECK(rcl_node_fini(&node))
	heap_caps_free(msg->format.data);
	heap_caps_free(msg->header.frame_id.data);
	heap_caps_free(msg->data.data);
	sensor_msgs__msg__CompressedImage__destroy(msg);
	vTaskDelete(NULL);
}
