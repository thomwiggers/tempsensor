#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"


// Settings

#define WEB_PORT "9091"
#define PUSH_HOST "cocytus"
#define PUSH_PATH "/metrics/job/pushgateway/"
#define METRIC_NAME "temperature_slaapkamer"


static const char *TAG = "pushing";


static const char *REQUEST_TEMPLATE = "POST " PUSH_PATH " HTTP/1.0\r\n"
      "Host: " PUSH_HOST ":" WEB_PORT "\r\n"
      "User-Agent: esp32/1.0 thom\r\n"
      "Content-Length: %d\r\n"
      "\r\n"
      "%s";

static const char *REQUEST_BODY_TEMPLATE =
      "#TYPE " METRIC_NAME " gauge\n"
      METRIC_NAME " %f\n";

struct in_addr addr = { .s_addr = 0 };
struct addrinfo *res;

int submit_temperature(float temp)
{
    const struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    int s, r;
    char recv_buf[64];

    while (addr.s_addr == 0) {
        int err = getaddrinfo(PUSH_HOST, WEB_PORT, &hints, &res);

        if(err != 0 || res == NULL) {
            ESP_LOGE(TAG, "DNS lookup failed err=%d res=%p", err, res);
            ESP_LOGE(TAG, "Goint to try again");
            vTaskDelay(2000 / portTICK_PERIOD_MS);
            continue;
        }

        /* Code to print the resolved IP.

Note: inet_ntoa is non-reentrant, look at ipaddr_ntoa_r for "real" code */
        addr = ((struct sockaddr_in *)res->ai_addr)->sin_addr;
        ESP_LOGI(TAG, "DNS lookup succeeded. IP=%s", inet_ntoa(addr));
    }
    ESP_LOGI(TAG, "Opening socket");

    s = socket(res->ai_family, res->ai_socktype, 0);
    if(s < 0) {
        ESP_LOGE(TAG, "... Failed to allocate socket.");
        addr.s_addr = 0;
        freeaddrinfo(res);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        return 0;
    }
    ESP_LOGI(TAG, "... allocated socket");

    if(connect(s, res->ai_addr, res->ai_addrlen) != 0) {
        ESP_LOGE(TAG, "... socket connect failed errno=%d", errno);
        addr.s_addr = 0;
        close(s);
        freeaddrinfo(res);
        return 0;
    }

    ESP_LOGI(TAG, "... connected");

    char body[100];
    int bytes = sprintf(body, REQUEST_BODY_TEMPLATE, temp);

    printf(REQUEST_TEMPLATE, bytes, body);

    if (dprintf(s, REQUEST_TEMPLATE, bytes, body) < 0) {
        ESP_LOGE(TAG, "... socket send failed");
        addr.s_addr = 0;
        close(s);
        freeaddrinfo(res);
        return 0;
    }
    ESP_LOGI(TAG, "... socket send success");

    struct timeval receiving_timeout;
    receiving_timeout.tv_sec = 5;
    receiving_timeout.tv_usec = 0;
    if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &receiving_timeout,
                sizeof(receiving_timeout)) < 0) {
        ESP_LOGE(TAG, "... failed to set socket receiving timeout");
        addr.s_addr = 0;
        close(s);
        freeaddrinfo(res);
        return 0;
    }
    ESP_LOGI(TAG, "... set socket receiving timeout success");

    /* Read HTTP response */
    do {
        bzero(recv_buf, sizeof(recv_buf));
        r = read(s, recv_buf, sizeof(recv_buf)-1);
        for(int i = 0; i < r; i++) {
            putchar(recv_buf[i]);
        }
    } while(r > 0);

    close(s);

    return 1;
}



