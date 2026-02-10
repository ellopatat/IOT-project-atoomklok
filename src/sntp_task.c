#include "sntp_task.h"

#include <string.h>
#include <stdint.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"

extern EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

static const char *TAG = "sntp_task";

#define NTP_SERVER   "pool.ntp.org"
#define NTP_PORT     "123"
//this value is the number of seconds between 1 Jan 1900 and 1 Jan 1970. NTP time is based on 1 Jan 1900, unix time is based on 1 Jan 1970.
#define NTP_TO_UNIX  2208988800UL


static uint8_t LI = 0, VN = 4, MODE = 3;

static int sntp_get_unix_time(uint32_t *unix_out)
{



    //build the NTP request. the first byte contains the LI, Version and MODE fields.
    //LI is the Leap Indicator, it is set to 0 because we are not using it.
    //VN is the Version Number, it is set to 4 because we are using NTPv4.
    //MODE is the Mode, it is set to 3 because we are a client requesting time-info.
    static uint8_t tx[48] = {0};
    tx[0] = (LI << 6) | (VN << 3) | MODE;


    /*the struct is used to filter for the type of address we want to get back from the DNS query, 
    in this case its IPv4 address(AF_INET) and socket (SOCK_DGRAM) for UDP. */
    struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_DGRAM,
    };
    //this is a struct. it will be filled by the getaddrinfo function with the IP and other info of the NTP server. 
    //hints is used as a filter for *res. res is a pointer because memory needs to be allocated for it which getaddrinfo uses.
    struct addrinfo *res = NULL;

    //getaddrinfo gets the dns info of the NTP server and fills the res struct with it.
    if (getaddrinfo(NTP_SERVER, NTP_PORT, &hints, &res) != 0 || res == NULL) {
        ESP_LOGW(TAG, "DNS failed");
        return -1;
    }
    //create socket information. It is used to tell the OS that we want to use certain types like IPv4 and UDP. 
    //The OS will then create a socket for us that we can use to send and receive data.
    int sock = socket(hints.ai_family, hints.ai_socktype, 0);
    if (sock < 0) {
        ESP_LOGW(TAG, "socket failed");
        freeaddrinfo(res);//free the memory allocated for res by getaddrinfo
        return -1;
    }


    //this code sends the actual request. 
    if (sendto(sock, tx, sizeof(tx), 0, res->ai_addr, res->ai_addrlen) < 0) {
        ESP_LOGW(TAG, "sendto failed");
        close(sock);
        freeaddrinfo(res);
        return -1;
    }

    //Receive response (48 bytes)
    //the NTP server will send back a response that is 48 bytes long. 
    //int flags is set to 0 because we don't need any special flags for receiving.
    //from and fromlen are set to NULL because we don't care about the source of the response, we just want the data.
    uint8_t rx[48] = {0};
    int len = recvfrom(sock, rx, sizeof(rx), 0, NULL, NULL);

    //close the socket and res to free the memory allocated for the socket and the res struct.
    close(sock);
    freeaddrinfo(res);

    if (len < 48) {
        ESP_LOGW(TAG, "No/short reply (%d)", len);
        return -1;
    }

    //converts the ntp_sec to an 32-bit unsigned integer. 
    //This is to prevent the compiler from promoting the uint8_t to an int and then shifting it which would cause the value to be incorrect.
    //so from BIG ENDIAN format to host byte order. The NTP time is in big endian format, most systems use little endian like the esp32.
    uint32_t ntp_sec_net;
    memcpy(&ntp_sec_net, &rx[40], sizeof(ntp_sec_net));
    uint32_t ntp_sec = ntohl(ntp_sec_net);

    //prevent ntp_sec values like 0 from messing with the unix time calculation.
    if (ntp_sec < NTP_TO_UNIX) return -1;

    //to get the correct time, we need to substract 70 years worth of seconds from the NTP time to get the Unix time.
    *unix_out = ntp_sec - NTP_TO_UNIX;
    ESP_LOGI(TAG, "NTP seconds: %lu, Unix seconds: %lu",
         (unsigned long)ntp_sec,
         (unsigned long)(*unix_out));
    return 0;
}

void sntp_task(void* arg){
    //wait for WiFi connection
    xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT,pdFALSE, pdTRUE, portMAX_DELAY);
    uint32_t *unix_time = (uint32_t *)arg;
    while (sntp_get_unix_time(unix_time) != 0) {
        ESP_LOGW(TAG, "Failed to get time from NTP server, retrying in 5 seconds...");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
    time_t now = *unix_time;
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);

    //converts the unix time to a human readable format. purely for debugging purposes.
    ESP_LOGI(TAG, "Successfully got time from NTP server: %lu", (unsigned long)*unix_time);
    ESP_LOGI("time", "%d-%d-%d %d:%d:%d UTC",
         timeinfo.tm_year + 1900,
         timeinfo.tm_mon + 1,
         timeinfo.tm_mday,
         timeinfo.tm_hour,
         timeinfo.tm_min,
         timeinfo.tm_sec);
    vTaskDelete(NULL);
}
