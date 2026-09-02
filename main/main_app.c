#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
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

#define SSID "XeloX@MESH"      // Router SSID to connect to
#define PASSWORD "P@1@nTiR"  // Router WPA2 password (min 8 chars)

#define MGMT_SERVER_HOST "hq-mmd-3.xelox.org"
#define MGMT_SERVER_PORT 5000
#define MGMT_REQUEST "STATUS"
#define MGMT_EVENT_MAX_LENGTH 32
#define MGMT_CLIENT_MAC_LENGTH 18
#define MGMT_CLIENT_NAME_LENGTH 20
#define MGMT_FULL_MESSAGE_LENGTH 96

// Area reserved for showing the server's last message and timestamp.
#define MGMT_DISPLAY_X 0
#define MGMT_DISPLAY_Y 200
#define MGMT_DISPLAY_W dispWidth
#define MGMT_DISPLAY_H 40

#define WIFI_MAXIMUM_RETRY 5

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

#define BACKLIGHT_TIMEOUT_MS 60000
static volatile uint32_t last_activity_ms = 0;
static volatile bool backlight_on = true;

/**
 * Get current timestamp in milliseconds since boot
 */
static uint32_t get_timestamp_ms(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

// Resets the inactivity timer and wakes the backlight if it was off.
static void note_activity(void)
{
  last_activity_ms = get_timestamp_ms();
  if (!backlight_on) {
    display_set_backlight(true);
    backlight_on = true;
    ESP_LOGI(TAG, "Backlight ON (button press)");
  }
}

void backlight_task(void *pvParameter)
{
  for (;;) {
    if (backlight_on && (get_timestamp_ms() - last_activity_ms) >= BACKLIGHT_TIMEOUT_MS) {
      display_set_backlight(false);
      backlight_on = false;
      ESP_LOGI(TAG, "Backlight OFF (idle %d ms)", BACKLIGHT_TIMEOUT_MS);
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
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

static int wifi_retry_count = 0;
static EventGroupHandle_t wifi_event_group;
static QueueHandle_t management_event_queue;
static SemaphoreHandle_t display_mutex;
#define WIFI_CONNECTED_BIT BIT0

// Derived once from the STA MAC address so the server can identify this device.
static char g_client_mac[MGMT_CLIENT_MAC_LENGTH] = {0};
static char g_client_name[MGMT_CLIENT_NAME_LENGTH] = {0};

static void init_client_identity(void)
{
  uint8_t mac[6] = {0};
  esp_err_t err = esp_wifi_get_mac(WIFI_IF_STA, mac);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Failed to read STA MAC address: %d", err);
  }
  snprintf(g_client_mac, sizeof(g_client_mac), "%02X:%02X:%02X:%02X:%02X:%02X",
    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  snprintf(g_client_name, sizeof(g_client_name), "ESP32-%02X%02X%02X", mac[3], mac[4], mac[5]);
  ESP_LOGI(TAG, "Client identity: %s (%s)", g_client_name, g_client_mac);
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
		esp_wifi_connect();
	} else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
		if (wifi_retry_count < WIFI_MAXIMUM_RETRY) {
			esp_wifi_connect();
			wifi_retry_count++;
			ESP_LOGI(TAG, "Retry connecting to AP (%d/%d)", wifi_retry_count, WIFI_MAXIMUM_RETRY);
		} else {
			ESP_LOGI(TAG, "Failed to connect to AP \"%s\"", SSID);
		}
	} else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
		ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
		wifi_retry_count = 0;
		ESP_LOGI(TAG, "Got IP: "IPSTR, IP2STR(&event->ip_info.ip));
    xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
	}
}

// Splits "message@timestamp" from the server and shows both on screen.
static void show_server_message(const char *response)
{
  char message[96];
  char timestamp[32] = {0};
  strncpy(message, response, sizeof(message) - 1);
  message[sizeof(message) - 1] = '\0';

  char *separator = strchr(message, '@');
  if (separator != NULL) {
    *separator = '\0';
    strncpy(timestamp, separator + 1, sizeof(timestamp) - 1);
  }

  xSemaphoreTake(display_mutex, portMAX_DELAY);
  display_fill_rect(MGMT_DISPLAY_X, MGMT_DISPLAY_Y, MGMT_DISPLAY_W, MGMT_DISPLAY_H, COLOR_BLACK);
  display_draw_text(MGMT_DISPLAY_X + 4, MGMT_DISPLAY_Y + 4, message, COLOR_WHITE, COLOR_BLACK, 1);
  if (timestamp[0] != '\0') {
    display_draw_text(MGMT_DISPLAY_X + 4, MGMT_DISPLAY_Y + 20, timestamp, COLOR_GREEN, COLOR_BLACK, 1);
  }
  xSemaphoreGive(display_mutex);
}

static void management_udp_task(void *params)
{
  char response[128];
  char message[MGMT_EVENT_MAX_LENGTH];

  for (;;) {
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);

    TickType_t wait_time = pdMS_TO_TICKS(10000);
    bool has_event = xQueueReceive(management_event_queue, message, wait_time) == pdTRUE;
    const char *request = has_event ? message : MGMT_REQUEST;

    char full_request[MGMT_FULL_MESSAGE_LENGTH];
    snprintf(full_request, sizeof(full_request), "%s|%s|%s", g_client_mac, g_client_name, request);

    struct addrinfo hints = {
      .ai_family = AF_INET,
      .ai_socktype = SOCK_DGRAM,
      .ai_protocol = IPPROTO_UDP,
    };
    struct addrinfo *server_info = NULL;
    char port_string[6];
    snprintf(port_string, sizeof(port_string), "%d", MGMT_SERVER_PORT);
    int resolve_result = getaddrinfo(MGMT_SERVER_HOST, port_string, &hints, &server_info);
    if (resolve_result != 0 || server_info == NULL) {
      ESP_LOGW(TAG, "Unable to resolve management server %s: error %d",
        MGMT_SERVER_HOST, resolve_result);
      vTaskDelay(pdMS_TO_TICKS(5000));
      continue;
    }

    int socket_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_fd < 0) {
      ESP_LOGE(TAG, "Unable to create UDP socket: errno %d", errno);
      freeaddrinfo(server_info);
      vTaskDelay(pdMS_TO_TICKS(5000));
      continue;
    }

    struct timeval receive_timeout = {
      .tv_sec = 2,
      .tv_usec = 0,
    };
    setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &receive_timeout, sizeof(receive_timeout));

    ssize_t sent = sendto(socket_fd, full_request, strlen(full_request), 0,
      server_info->ai_addr, server_info->ai_addrlen);
    if (sent < 0) {
      ESP_LOGE(TAG, "UDP send failed: errno %d", errno);
    } else {
      ssize_t received = recvfrom(socket_fd, response, sizeof(response) - 1, 0, NULL, NULL);
      if (received < 0) {
        ESP_LOGW(TAG, "No UDP response from %s:%d: errno %d",
          MGMT_SERVER_HOST, MGMT_SERVER_PORT, errno);
      } else {
        response[received] = '\0';
        ESP_LOGI(TAG, "Management server response to %s: %s", request, response);
        show_server_message(response);
      }
    }

    shutdown(socket_fd, SHUT_RDWR);
    close(socket_fd);
    freeaddrinfo(server_info);
    if (!has_event) {
      vTaskDelay(pdMS_TO_TICKS(10000));
    }
  }
}

static void queue_button_event(const char *event)
{
  char queued_event[MGMT_EVENT_MAX_LENGTH] = {0};
  strncpy(queued_event, event, sizeof(queued_event) - 1);
  if (xQueueSend(management_event_queue, queued_event, 0) != pdTRUE) {
    ESP_LOGW(TAG, "Management event queue full, dropping %s", event);
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
      note_activity();
      xSemaphoreTake(display_mutex, portMAX_DELAY);
      display_fill_rect(20, 100, 80, 40, COLOR_YELLOW);
      snprintf(display_text, sizeof(display_text), "B1:%lu", button1_count);
      display_draw_text(25, 108, display_text, COLOR_BLACK, COLOR_YELLOW, 1);
      xSemaphoreGive(display_mutex);
      queue_button_event("BUTTON1_PRESSED");
    }

    // Detect button2 press (falling edge: 1 -> 0)
    if (ButtonStates.button2_old && !ButtonStates.button2)
    {
      button2_count++;
      button2_last_time = get_timestamp_ms();
      ESP_LOGI("BUTTON", "Button 2 PRESSED! Count: %lu, Time: %lums", button2_count, button2_last_time);
      note_activity();
      xSemaphoreTake(display_mutex, portMAX_DELAY);
      display_fill_rect(140, 100, 80, 40, COLOR_CYAN);
      snprintf(display_text, sizeof(display_text), "B2:%lu", button2_count);
      display_draw_text(150, 108, display_text, COLOR_BLACK, COLOR_CYAN, 1);
      xSemaphoreGive(display_mutex);
      queue_button_event("BUTTON2_PRESSED");
    }

    // Detect button3 press (falling edge: 1 -> 0)
    if (ButtonStates.button3_old && !ButtonStates.button3)
    {
      button3_count++;
      button3_last_time = get_timestamp_ms();
      ESP_LOGI("BUTTON", "Button 3 PRESSED! Count: %lu, Time: %lums", button3_count, button3_last_time);
      note_activity();
      xSemaphoreTake(display_mutex, portMAX_DELAY);
      display_fill_rect(260, 100, 80, 40, COLOR_MAGENTA);
      snprintf(display_text, sizeof(display_text), "B3:%lu", button3_count);
      display_draw_text(270, 108, display_text, COLOR_BLACK, COLOR_MAGENTA, 1);
      xSemaphoreGive(display_mutex);
      queue_button_event("BUTTON3_PRESSED");
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
  wifi_event_group = xEventGroupCreate();
  if (wifi_event_group == NULL) {
    ESP_LOGE(TAG, "Failed to create Wi-Fi event group");
    return;
  }
   management_event_queue = xQueueCreate(8, MGMT_EVENT_MAX_LENGTH);
   if (management_event_queue == NULL) {
     ESP_LOGE(TAG, "Failed to create management event queue");
     return;
   }
   display_mutex = xSemaphoreCreateMutex();
   if (display_mutex == NULL) {
     ESP_LOGE(TAG, "Failed to create display mutex");
     return;
   }
   buttons_init();
   printf("Buttons initialized\n");

   printf("Display init\n");
   display_init();
   printf("Display initialized successfully\n");
   last_activity_ms = get_timestamp_ms();
   
   display_fill_screen(COLOR_BLACK);
   printf("Screen filled\n");
   
   display_draw_pixel(100, 100, COLOR_BLUE);
   display_draw_pixel(100, 200, COLOR_YELLOW);
   printf("Pixels drawn\n");

   printf("Button task creating\n");
   xTaskCreate(&fetchButtontask, "button fetching", 2048, "task 1", 2, NULL);

   printf("Backlight task creating\n");
   xTaskCreate(&backlight_task, "backlight", 2048, NULL, 1, NULL);
  
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
   // Default STA netif requests an IP from the router's DHCP server
   esp_netif_create_default_wifi_sta();

   wifi_init_config_t wifiInitializationConfig = WIFI_INIT_CONFIG_DEFAULT();
 
   printf("WiFi init config done\n");
   ESP_ERROR_CHECK(esp_wifi_init(&wifiInitializationConfig));

   ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
   ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));
 
   printf("WiFi mode\n");
   ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
 
   wifi_config_t sta_config = {
          .sta = {
            .ssid = SSID,
            .password = PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
          }
        };
   
   printf("WiFi STA config: connecting to SSID=%s\n", SSID);
 
   printf("WiFi config set\n");
   ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
 
   printf("WiFi start\n");
   ESP_ERROR_CHECK(esp_wifi_start());

  init_client_identity();
  xTaskCreate(&management_udp_task, "management_udp", 4096, NULL, 4, NULL);

   printf("App main loop starting - Ready to test buttons!\n");

  while (1)
  {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
