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
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "display.h"

#define SSID "XeloX@MESH"
#define PASSWORD "P@1@nTiR"  // WPA2 password (min 8 chars)

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
	// Note: GPIO 39, 37 and 38 are input-only pads on this chip (no internal pull-ups)
	// Rely on external pull-up resistors on the board
	gpio_set_direction(PIN_BUTTON1, GPIO_MODE_INPUT);
	// gpio_pullup_en(PIN_BUTTON1);  // Not supported on ADC pins

	gpio_set_direction(PIN_BUTTON2, GPIO_MODE_INPUT);
	// gpio_pullup_en(PIN_BUTTON2);  // Not supported on ADC pins

	gpio_set_direction(PIN_BUTTON3, GPIO_MODE_INPUT);
	// gpio_pullup_en(PIN_BUTTON3);  // Not supported on this input-only pad
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
		wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *) event_data;
		ESP_LOGI(TAG, "Station "MACSTR" joined, AID=%d", MAC2STR(event->mac), event->aid);
	} else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
		wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *) event_data;
		ESP_LOGI(TAG, "Station "MACSTR" left, AID=%d", MAC2STR(event->mac), event->aid);
	} else if (event_base == IP_EVENT && event_id == IP_EVENT_AP_STAIPASSIGNED) {
		ip_event_ap_staipassigned_t *event = (ip_event_ap_staipassigned_t *) event_data;
		ESP_LOGI(TAG, "Assigned IP "IPSTR" to station "MACSTR, IP2STR(&event->ip), MAC2STR(event->mac));
	}
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

   printf("NVS init\n");
   esp_err_t nvs_ret = nvs_flash_init();
   if (nvs_ret == ESP_ERR_NVS_NO_FREE_PAGES || nvs_ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
     ESP_ERROR_CHECK(nvs_flash_erase());
     nvs_ret = nvs_flash_init();
   }
   ESP_ERROR_CHECK(nvs_ret);

   printf("WiFi init\n");
   ESP_ERROR_CHECK(esp_netif_init());
   ESP_ERROR_CHECK(esp_event_loop_create_default());
   // Default AP netif starts its own DHCP server on 192.168.4.1/24
   esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();

   wifi_init_config_t wifiInitializationConfig = WIFI_INIT_CONFIG_DEFAULT();
 
   printf("WiFi init config done\n");
   ESP_ERROR_CHECK(esp_wifi_init(&wifiInitializationConfig));

   ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
   ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_AP_STAIPASSIGNED, &wifi_event_handler, NULL, NULL));
 
   printf("WiFi mode\n");
   ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
 
   wifi_config_t ap_config = {
          .ap = {
            .ssid = SSID,
            .ssid_len = strlen(SSID),
            .password = PASSWORD,
            .channel = 0,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .ssid_hidden = 0,
            .max_connection = 4,
            .beacon_interval = 100
          }
        };
   
   printf("WiFi AP config: SSID=%s, AuthMode=WPA2_PSK, MaxConnections=4\n", SSID);
 
   printf("WiFi config set\n");
   ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
 
   printf("WiFi start\n");
   ESP_ERROR_CHECK(esp_wifi_start());

   esp_netif_ip_info_t ip_info;
   esp_netif_get_ip_info(ap_netif, &ip_info);
   ESP_LOGI(TAG, "AP IP address: "IPSTR" (DHCP server enabled)", IP2STR(&ip_info.ip));
   
   printf("App main loop starting - Ready to test buttons!\n");

  while (1)
  {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
