#include "http_server.h"

#include "esp_log.h"

#include "led.h"


static const char *TAG = "HTTP";


static esp_err_t led_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "GET /led received");

    const char *response = led_is_on() ? "LED: ON" : "LED: OFF";

    return httpd_resp_send(
        req,
        response,
        HTTPD_RESP_USE_STRLEN
    );
}


static esp_err_t led_toggle_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "GET /led/toggle received");

    led_toggle(LED);

    const char *response = led_is_on() ? "LED: ON" : "LED: OFF";

    return httpd_resp_send(
        req,
        response,
        HTTPD_RESP_USE_STRLEN
    );
}


static esp_err_t root_handler(httpd_req_t *req)
{
    const char *html =
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<title>Smart Home</title>"
        "</head>"

        "<body>"

        "<h1>Smart Home</h1>"

        "<h2 id=\"led-status\">LED: </h2>"

        "<button id=\"led-toggle\">Toggle LED</button>"

        "<script>"

        "fetch(\"/led\")"
        ".then(response => response.text())"
        ".then(data => {"
            "document.getElementById(\"led-status\").textContent = data;"
        "});"

        "const btn = document.getElementById(\"led-toggle\");"

        "btn.addEventListener('click', function() {"
            "fetch(\"/led/toggle\")"
            ".then(response => response.text())"
            ".then(data => {"
                "document.getElementById(\"led-status\").textContent = data;"
            "});"
        "});"

        "</script>"

        "</body>"
        "</html>";


    httpd_resp_set_type(req, "text/html");

    return httpd_resp_send(
        req,
        html,
        HTTPD_RESP_USE_STRLEN
    );
}


static const httpd_uri_t root_uri =
{
    .uri = "/",
    .method = HTTP_GET,
    .handler = root_handler,
    .user_ctx = NULL
};


static const httpd_uri_t led_uri =
{
    .uri = "/led",
    .method = HTTP_GET,
    .handler = led_handler,
    .user_ctx = NULL
};


static const httpd_uri_t led_toggle_uri =
{
    .uri = "/led/toggle",
    .method = HTTP_GET,
    .handler = led_toggle_handler,
    .user_ctx = NULL
};


httpd_handle_t http_server_start(void)
{
    httpd_handle_t server = NULL;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    esp_err_t err = httpd_start(&server, &config);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(err));
        return NULL;
    }


    httpd_register_uri_handler(server, &root_uri);
    httpd_register_uri_handler(server, &led_uri);
    httpd_register_uri_handler(server, &led_toggle_uri);


    ESP_LOGI(TAG, "HTTP server started");

    return server;
}