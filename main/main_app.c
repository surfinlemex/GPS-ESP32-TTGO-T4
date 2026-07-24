#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "console/console.h"
//#include "console/console.c"
#include "ili9341/fonts/font.h"
#include "ili9341/fonts/f16f.h"
#include "ili9341/fonts/f24f.h"
#include "ili9341/fonts/f32f.h"
#include "ili9341/fonts/f6x8m.h"
#include "esp_log.h"
#include "esp_system.h"

//#include "ili9341/fonts/font.c"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_psram.h"
#include "ili9341/ili9341.h"
//#include "ili9341/ili9341.c"
#include "esp32/himem.h"
#include "esp_wifi.h"

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


void buttons_init()
{
	gpio_set_direction(PIN_BUTTON1, GPIO_MODE_INPUT);
	gpio_pullup_en(PIN_BUTTON1);
	gpio_set_direction(PIN_BUTTON2, GPIO_MODE_INPUT);
	gpio_pullup_en(PIN_BUTTON2);
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
  while (true)
  {
    ButtonStates.button1 = gpio_get_level(PIN_BUTTON1);
    ButtonStates.button2 = gpio_get_level(PIN_BUTTON2);
    ButtonStates.button3 = gpio_get_level(PIN_BUTTON3);

    ESP_LOGI("TASK_1","waiting for button press %s\n", (char *) params);
//    vTaskDelay(1000 / portTICK_PERIOD_MS);
    vTaskDelay(10000 / portTICK_PERIOD_MS);
    if (uxTaskGetStackHighWaterMark(NULL) < 10)
       ESP_LOGW(TASK1_TAG,"Close to running out of stack space!\n");
  }
}


void app_main()
{
   buttons_init();

   printf("Display init\n");
   ili9341_init(dispWidth, dispHeight);
   ili9341_SetBL(100);
   ili9341_FillScreen(BLACK);
   ili9341_DrawPixel(100, 100, BLUE);
   ili9341_DrawPixel(100, 200, YELLOW);

   ili9341_TextOutput(20, 20, 0, RED, "Hello world!!!");
   ili9341_DrawCircle(100, 100, 60, GREEN);

//   xTaskCreatePinnedToCore(&fetchButtontask, "button fetching", 2048, "task 1", 2, NULL,2);
   xTaskCreate(&fetchButtontask, "button fetching", 2048, "task 1", 2, NULL);
//   xTaskCreate(&fetchButtontask, "button fetching", 2048, NULL, tskIDLE_PRIORITY, NULL);
  
   xTaskCreatePinnedToCore(&monitoring_task, "monitoring_task", 2048, NULL, 1, NULL, 1);


    wifi_init_config_t wifiInitializationConfig = WIFI_INIT_CONFIG_DEFAULT();
 
    esp_wifi_init(&wifiInitializationConfig);
 
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
 
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
 
    esp_wifi_set_config(WIFI_IF_AP, &ap_config);
 
    esp_wifi_start();

  while (1)
  {
  }
}
