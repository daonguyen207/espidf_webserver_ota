/* Thư viện espidf_webserver_ota
   Người viết: Daonguyen IoT47
   Email: daonguyen20798@gmail.com
   Website: Iot47.com
   Link thư viện: https://github.com/daonguyen207/espidf_webserver_ota
   Sử dụng: 
     1. Khởi tạo webserver theo ví dụ của idf
     2. Include thư viện vào project
     3. Gọi hàm webserver_ota_init với tham số là địa chỉ của server
     4. Truy cập yourip/update để tải firmware lên ( chú ý yourip là địa chỉ ip của bạn)
*/
#ifndef _IOT47_WIFI_OTA_H_
#define _IOT47_WIFI_OTA_H_

#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "esp_netif.h"
#include "esp_ota_ops.h"
#include "esp_flash_partitions.h"
#include "esp_partition.h"
#include <esp_http_server.h>

void webserver_ota_init(httpd_handle_t *server);

#endif