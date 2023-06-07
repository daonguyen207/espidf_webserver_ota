#include "espidf_webserver_ota.h"
#include "ota_index_page.h"

static const char *TAG = "ota";

uint8_t ota_reset_state = 0;
void ota_reset_task(void *pvParameters)
{
    for(;;)
    {
        if(ota_reset_state)
        {
            ESP_LOGI(TAG, "Reset...");
            vTaskDelay(500/portTICK_PERIOD_MS);
            esp_restart();
            vTaskDelete(NULL); 
        }
        vTaskDelay(1000/portTICK_PERIOD_MS);
    }
}

static esp_err_t ota_index_get_handler(httpd_req_t *req)
{
    const char* resp_str = (const char*) ota_index_page;
    httpd_resp_send(req, resp_str, sizeof(ota_index_page));

    /* After sending the HTTP response the old HTTP request
     * headers are lost. Check if HTTP request headers can be read now. */
    if (httpd_req_get_hdr_value_len(req, "Host") == 0) {
        ESP_LOGI(TAG, "Request headers lost");
    }
    return ESP_OK;
}

static const httpd_uri_t ota_http_index = {
    .uri       = "/update",
    .method    = HTTP_GET,
    .handler   = ota_index_get_handler,
};




#define SCRATCH_BUFSIZE  8192
char scratch[SCRATCH_BUFSIZE];
static esp_err_t upload_post_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG,"fimrware size:  %i",req->content_len);
    char *buf = req->user_ctx;
    int received;
    int remaining = req->content_len;
    
    //chuẩn bị ota
    esp_err_t err;
    esp_ota_handle_t update_handle = 0 ;
    const esp_partition_t *update_partition = NULL;
    const esp_partition_t *configured = esp_ota_get_boot_partition();
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (configured != running) {
        ESP_LOGW(TAG, "Configured OTA boot partition at offset 0x%08x, but running from offset 0x%08x",configured->address, running->address);
        ESP_LOGW(TAG, "(This can happen if either the OTA boot data or preferred boot image become corrupted somehow.)");
    }
    ESP_LOGI(TAG, "Running partition type %d subtype %d (offset 0x%08x)",running->type, running->subtype, running->address);
    update_partition = esp_ota_get_next_update_partition(NULL);
    assert(update_partition != NULL);
    ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%x",update_partition->subtype, update_partition->address);
    err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &update_handle);

    if (err != ESP_OK) {
        esp_ota_abort(update_handle);
        ESP_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "esp_ota_begin failed"); 
        return ESP_FAIL;
    }

    while (remaining > 0) 
    {
        /* Receive the file part by part into a buffer */
        if ((received = httpd_req_recv(req, buf, MIN(remaining, SCRATCH_BUFSIZE))) <= 0) {
            if (received == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }

            esp_ota_abort(update_handle);
            ESP_LOGE(TAG, "Không thể tiếp nhận tệp!");
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Không thể tiếp nhận tệp!"); 
            return ESP_FAIL;
        }

        if(received)
        {
            ESP_LOGI(TAG, "received size : %d", received);

            err = esp_ota_write( update_handle, (const void *)buf, received);
            if (err != ESP_OK) {
                esp_ota_abort(update_handle);
                ESP_LOGE(TAG, "Lỗi khi ghi tệp vào bộ nhớ");
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Lỗi khi ghi tệp vào bộ nhớ"); 
                return ESP_FAIL;
            }
        }
        remaining -= received;
    }

    /* Close file upon upload completion */
    ESP_LOGI(TAG, "Download file complete");

    err = esp_ota_end(update_handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
            ESP_LOGE(TAG, "Image validation failed, image is corrupted");
        } else {
            ESP_LOGE(TAG, "esp_ota_end failed (%s)!", esp_err_to_name(err));
        }
        ESP_LOGE(TAG, "Lỗi khi kết thúc ota");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Lỗi khi kết thúc ota"); 
        return ESP_FAIL;
    }
    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed (%s)!", esp_err_to_name(err));
        ESP_LOGE(TAG, "Lỗi khi set boot partition");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Lỗi khi set boot partition"); 
        return ESP_FAIL;
    }

    //ok hết
    httpd_resp_set_status(req, "200 OK");
    httpd_resp_sendstr(req, "OK");
    ota_reset_state = 1;
    return ESP_OK;
}

httpd_uri_t file_upload = {
        .uri       = "/upload",   // Match all URIs of type /upload/path/to/file
        .method    = HTTP_POST,
        .handler   = upload_post_handler,
        .user_ctx  = scratch    // Pass server data as context
    };

void webserver_ota_init(httpd_handle_t *server)
{
  xTaskCreate(&ota_reset_task, "ota_reset_task", 1024, NULL, 5, NULL);
  httpd_register_uri_handler(*server, &ota_http_index);
  httpd_register_uri_handler(*server, &file_upload);  
}
