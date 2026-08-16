#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_psram.h"
#include "esp32/himem.h"
#include "esp_wifi.h"
#include "display.h"

#define SSID "ESP32AP"

#define SW_VERSION_MAJOR	1
#define SW_VERSION_MINOR	0

#define TASK1_TAG "TASK_1"

// Screen size
#define dispWidth	320
#define dispHeight	240


#define PIN_BUTTON1 	39
#define PIN_BUTTON2 	37
#define PIN_BUTTON3	38

#define BUFF_SIZE	dispWidth

static const char TAG[] = "main";

struct sButtonStates
{
	uint8_t	button1     :1;
	uint8_t	button2     :1;
	uint8_t	button3     :1;
	uint8_t	button1_old :1;
	uint8_t	button2_old :1;
	uint8_t	button3_old :1;
};

typedef enum Mode
{
	SelectMode = 0,
	SelectRate = 1,
	SelectLedCurrent = 2
} eMode;

#define ButtonsModeNum		3
eMode ButtonsMode = SelectMode;

// Switch LED current
#define LedCurrentNum		16
const float LedCurrents[LedCurrentNum] = {0.0, 4.4, 7.6, 11.0, 14.2, 17.4, 20.8, 24.0, 27.1, 30.6, 33.8, 37.0, 40.2, 43.6, 46.8, 50.0};
uint8_t CurLedCurrent = 7;	//24

#define MODE_MA	7
uint8_t CurrentMode = 0;

// Button press counters and timestamps
static uint32_t button1_count = 0;
static uint32_t button2_count = 0;
static uint32_t button3_count = 0;
static uint32_t button1_last_time = 0;
static uint32_t button2_last_time = 0;
static uint32_t button3_last_time = 0;

/**
 * Get current timestamp in milliseconds since boot
 */
static uint32_t get_timestamp_ms(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

void buttons_init()
{
	// Configure button pins as inputs
	// Note: GPIO 39 and 37 are ADC-only pins with input-only pads (no internal pull-ups)
	// Use external pull-up resistors or rely on board design
	gpio_set_direction(PIN_BUTTON1, GPIO_MODE_INPUT);
	// gpio_pullup_en(PIN_BUTTON1);  // Not supported on ADC pins
	
	gpio_set_direction(PIN_BUTTON2, GPIO_MODE_INPUT);
	// gpio_pullup_en(PIN_BUTTON2);  // Not supported on ADC pins
	
	gpio_set_direction(PIN_BUTTON3, GPIO_MODE_INPUT);
	gpio_pullup_en(PIN_BUTTON3);
}
void monitoring_task(void *pvParameter)
{
	for(;;){
		ESP_LOGI(TAG, "free heap: %d",esp_get_free_heap_size());
		vTaskDelay( pdMS_TO_TICKS(10000) );
	}
}



void fetchButtontask(void * params)
{
  struct sButtonStates ButtonStates;
  char display_text[20];
  memset(&ButtonStates, 1, sizeof(ButtonStates));  // Initialize to 1 (not pressed)
  
  while (true)
  {
    ButtonStates.button1 = gpio_get_level(PIN_BUTTON1);
    ButtonStates.button2 = gpio_get_level(PIN_BUTTON2);
    ButtonStates.button3 = gpio_get_level(PIN_BUTTON3);

    // Detect button1 press (falling edge: 1 -> 0)
    if (ButtonStates.button1_old && !ButtonStates.button1)
    {
      button1_count++;
      button1_last_time = get_timestamp_ms();
      ESP_LOGI("BUTTON", "Button 1 PRESSED! Count: %lu, Time: %lums", button1_count, button1_last_time);
      display_fill_rect(20, 100, 80, 40, COLOR_YELLOW);
      snprintf(display_text, sizeof(display_text), "B1:%lu", button1_count);
      display_draw_text(25, 108, display_text, COLOR_BLACK, COLOR_YELLOW, 1);
    }

    // Detect button2 press (falling edge: 1 -> 0)
    if (ButtonStates.button2_old && !ButtonStates.button2)
    {
      button2_count++;
      button2_last_time = get_timestamp_ms();
      ESP_LOGI("BUTTON", "Button 2 PRESSED! Count: %lu, Time: %lums", button2_count, button2_last_time);
      display_fill_rect(140, 100, 80, 40, COLOR_CYAN);
      snprintf(display_text, sizeof(display_text), "B2:%lu", button2_count);
      display_draw_text(150, 108, display_text, COLOR_BLACK, COLOR_CYAN, 1);
    }

    // Detect button3 press (falling edge: 1 -> 0)
    if (ButtonStates.button3_old && !ButtonStates.button3)
    {
      button3_count++;
      button3_last_time = get_timestamp_ms();
      ESP_LOGI("BUTTON", "Button 3 PRESSED! Count: %lu, Time: %lums", button3_count, button3_last_time);
      display_fill_rect(260, 100, 80, 40, COLOR_MAGENTA);
      snprintf(display_text, sizeof(display_text), "B3:%lu", button3_count);
      display_draw_text(270, 108, display_text, COLOR_BLACK, COLOR_MAGENTA, 1);
    }

    // Update old states for next iteration
    ButtonStates.button1_old = ButtonStates.button1;
    ButtonStates.button2_old = ButtonStates.button2;
    ButtonStates.button3_old = ButtonStates.button3;

    vTaskDelay(pdMS_TO_TICKS(50));  // 50ms debounce delay
    if (uxTaskGetStackHighWaterMark(NULL) < 10)
       ESP_LOGW(TASK1_TAG,"Close to running out of stack space!\n");
  }
}


void app_main()
{
   printf("App main started\n");
   buttons_init();
   printf("Buttons initialized\n");

   printf("Display init\n");
   display_init();
   printf("Display initialized successfully\n");
   
   display_fill_screen(COLOR_BLACK);
   printf("Screen filled\n");
   
   display_draw_pixel(100, 100, COLOR_BLUE);
   display_draw_pixel(100, 200, COLOR_YELLOW);
   printf("Pixels drawn\n");

   printf("Button task creating\n");
   xTaskCreate(&fetchButtontask, "button fetching", 2048, "task 1", 2, NULL);
  
   printf("Monitoring task creating\n");
   xTaskCreatePinnedToCore(&monitoring_task, "monitoring_task", 2048, NULL, 1, NULL, 1);

   printf("WiFi init\n");
   wifi_init_config_t wifiInitializationConfig = WIFI_INIT_CONFIG_DEFAULT();
 
   printf("WiFi init config done\n");
   esp_wifi_init(&wifiInitializationConfig);
 
   printf("WiFi storage\n");
   esp_wifi_set_storage(WIFI_STORAGE_RAM);
 
   printf("WiFi mode\n");
   esp_wifi_set_mode(WIFI_MODE_AP);
 
   wifi_config_t ap_config = {
          .ap = {
            .ssid = SSID,
            .channel = 0,
            .authmode = WIFI_AUTH_OPEN,
            .ssid_hidden = 0,
            .max_connection = 1,
            .beacon_interval = 100
          }
        };
 
   printf("WiFi config set\n");
   esp_wifi_set_config(WIFI_IF_AP, &ap_config);
 
   printf("WiFi start\n");
   esp_wifi_start();
   
   printf("App main loop starting - Ready to test buttons!\n");

  while (1)
  {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
