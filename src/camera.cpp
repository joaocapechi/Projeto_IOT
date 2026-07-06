#include <Arduino.h>
#include <ArduinoJson.h>

/*
********************************************************************************************
******************************************* Wifi *******************************************
********************************************************************************************
*/
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "certificados.h"

void reconectarWiFi() {
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.begin("LabIoT", "4n1m4l5@))!!");
        Serial.print("Conectando ao WiFi...");
        while (WiFi.status() != WL_CONNECTED) {
            Serial.print(".");
            delay(1000);
        }
        Serial.print("conectado!\nEndereço IP: ");
        Serial.println(WiFi.localIP());
    }
} 


/*
********************************************************************************************
****************************************** Camera ******************************************
********************************************************************************************
*/
#include <esp_camera.h>

camera_config_t config = {
    .pin_pwdn = -1, .pin_reset = -1,
    .pin_xclk = 15, .pin_sscb_sda = 4,
    .pin_sscb_scl = 5,
    .pin_d7 = 16, .pin_d6 = 17,
    .pin_d5 = 18, .pin_d4 = 12,
    .pin_d3 = 10, .pin_d2 = 8,
    .pin_d1 = 9, .pin_d0 = 11,
    .pin_vsync = 6, .pin_href = 7,
    .pin_pclk = 13,
    .xclk_freq_hz = 20000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,
    .pixel_format = PIXFORMAT_JPEG,
    .frame_size = FRAMESIZE_SVGA, // (800 x 600)
    .jpeg_quality = 12, .fb_count = 1,
    .fb_location = CAMERA_FB_IN_PSRAM,
    .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
}; 


/*
*********************************************************************************************
*************************************** Processamento ***************************************
*********************************************************************************************
*/
#include <lab_human_detection_inferencing.h>
#include "edge-impulse-sdk/dsp/image/image.hpp"

static uint8_t *snapshot_buf = nullptr;

#define EI_CAMERA_RAW_FRAME_BUFFER_COLS 320
#define EI_CAMERA_RAW_FRAME_BUFFER_ROWS 240
#define EI_CAMERA_FRAME_BYTE_SIZE 3

static int ei_camera_get_data(size_t offset, size_t length, float *out_ptr)
{
    // Re-pack RGB888 bytes back into Edge Impulse standard format
    size_t pixel_ix = offset * 3;
    size_t pixels_left = length;
    size_t out_ptr_ix = 0;

    while (pixels_left != 0) {
        // Edge Impulse expect RGB components extracted cleanly
        out_ptr[out_ptr_ix] = (snapshot_buf[pixel_ix + 0] << 16) + 
                              (snapshot_buf[pixel_ix + 1] << 8) + 
                              snapshot_buf[pixel_ix + 2];
        out_ptr_ix++;
        pixel_ix += 3;
        pixels_left--;
    }
    return 0;
}

// MQTT
#include <MQTT.h> 
WiFiClientSecure conexaoSegura;
MQTTClient mqtt(1000); 

void reconectarMQTT() {
    if (!mqtt.connected()) {
        Serial.print("Conectando MQTT...");
        while(!mqtt.connected()) {
            mqtt.connect("ProjIOTCam", "aula", "zowmad-tavQez");
            Serial.print(".");
            delay(1000);
        }
        Serial.println(" conectado!");
    }
}


void tirarFotoEEnviarParaMQTT () {
    camera_fb_t* foto = esp_camera_fb_get();
    if (!foto) {
        Serial.println("Erro ao tirar foto");
        return;
    }

    if (mqtt.publish("A1/esp32/camera", (const char*)foto->buf, foto->len)) Serial.println("Foto enviada com sucesso");
    else Serial.println("Falha ao enviar foto");

    size_t required_size = foto->width * foto->height * EI_CAMERA_FRAME_BYTE_SIZE;
    snapshot_buf = (uint8_t*)ps_malloc(required_size);
    if (!snapshot_buf) {
        Serial.println("Erro ao alocar memória");
        esp_camera_fb_return(foto);
        return;
    }

    bool converted = fmt2rgb888(
        foto->buf,
        foto->len,
        PIXFORMAT_JPEG,
        snapshot_buf
    );

    // Free buffer of cam
    esp_camera_fb_return(foto);

    if (!converted) {
        Serial.println("JPEG -> RGB conversion failed");
        free(snapshot_buf);
        return;
    }

    // Resize if model input differs from QVGA
    if (EI_CLASSIFIER_INPUT_WIDTH != foto->width ||
        EI_CLASSIFIER_INPUT_HEIGHT != foto->height) {
        ei::image::processing::crop_and_interpolate_rgb888(
            snapshot_buf,
            foto->width,   // <-- Real width
            foto->height,  // <-- Real height
            snapshot_buf,
            EI_CLASSIFIER_INPUT_WIDTH,
            EI_CLASSIFIER_INPUT_HEIGHT
        );
    }

    // Feed frame to Edge Impulse
    signal_t signal;
    signal.total_length = EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT;

    signal.get_data = &ei_camera_get_data;

    ei_impulse_result_t result = { 0 };

    EI_IMPULSE_ERROR err = run_classifier(&signal, &result, false);

    if (err != EI_IMPULSE_OK) {
        Serial.print("Failed to run classifier");
        Serial.println(err);

        free(snapshot_buf);
        snapshot_buf = nullptr;
        return;
    }

    int count_person = 0;
    ei_printf("Predictions (DSP: %d ms., Classification: %d ms., Anomaly: %d ms.): \n",
                result.timing.dsp, result.timing.classification, result.timing.anomaly);
    
    JsonDocument coords;
    for (size_t ix = 0; ix < result.bounding_boxes_count; ix++) {
        auto &bb = result.bounding_boxes[ix];
        if (bb.value == 0) continue;

        Serial.printf(
            "%s %.2f x=%u y=%u w=%u h=%u\n",
            bb.label,
            bb.value,
            bb.x,
            bb.y,
            bb.width,
            bb.height
        );

        if (bb.value > 0.75f) {
            JsonDocument coord;
            count_person++;
        }
    }

    String data = String(count_person);
    mqtt.publish("A1/esp32/camera/qtd", data);

    free(snapshot_buf);
    snapshot_buf = nullptr;
}


/*
********************************************************************************************
*************************************** SetUp e Loop ***************************************
********************************************************************************************
*/
unsigned long long lastTime = 0;

void setup() {
    Serial.begin(115200); Serial.println("Starting...");

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Erro na câmera: 0x%x\n", err);
        while (true);
    }
    reconectarWiFi();
    conexaoSegura.setCACert(certificado1);
    mqtt.begin("mqtt.janks.dev.br", 8883, conexaoSegura);
    mqtt.setKeepAlive(100);
    reconectarMQTT();
}


void loop() {
    reconectarWiFi();
    reconectarMQTT();
    mqtt.loop(); 

    if (millis() - lastTime >= 2000) {
        tirarFotoEEnviarParaMQTT();
        lastTime = millis();
    }
}