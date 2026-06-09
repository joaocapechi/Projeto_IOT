#include <Arduino.h>

// Câmera
#include <esp_camera.h>

// Processamento
#include <Person_Detection_-_ESP32_inferencing.h> // from exported .zip
#include "edge-impulse-sdk/dsp/image/image.hpp"

// MQTT
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "certificados.h"
#include <MQTT.h> 

// Config cam
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
 .frame_size = FRAMESIZE_QVGA, // FRAMESIZE_SVGA,
 .jpeg_quality = 12, .fb_count = 1,
 .fb_location = CAMERA_FB_IN_PSRAM,
 .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
//  .grab_mode = CAMERA_GRAB_WHEN_EMPTY // CAMERA_GRAB_LATEST
}; 

// Processamento
static uint8_t *snapshot_buf = nullptr;

#define EI_CAMERA_RAW_FRAME_BUFFER_COLS 320
#define EI_CAMERA_RAW_FRAME_BUFFER_ROWS 240
#define EI_CAMERA_FRAME_BYTE_SIZE 3

static int ei_camera_get_data(size_t offset, size_t length, float *out_ptr)
{
    size_t pixel_ix = offset * 3;
    size_t pixels_left = length;
    size_t out_ptr_ix = 0;

    while (pixels_left != 0) {
        out_ptr[out_ptr_ix] = (snapshot_buf[pixel_ix + 2] << 16) +
            (snapshot_buf[pixel_ix + 1] << 8) +
            snapshot_buf[pixel_ix];
        out_ptr_ix++;
        pixel_ix += 3;
        pixels_left--;
    }

    return 0;
}

// MQTT
WiFiClientSecure conexaoSegura;
MQTTClient mqtt(1000); 

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

void reconectarMQTT() {
    if (!mqtt.connected()) {
        Serial.print("Conectando MQTT...");
        while(!mqtt.connected()) {
            mqtt.connect("ProjIOTCam", "aula", "zowmad-tavQez");
            Serial.print(".");
            delay(1000);
        }
        Serial.println(" conectado!");

        // mqtt.subscribe("iot/camera"); // qos = 0
        // mqtt.subscribe("topico2/+/parametro", 1); // qos = 1
    }
}

void tirarFotoEEnviarParaMQTT () {
    camera_fb_t* foto = esp_camera_fb_get();
    if (!foto) {
        Serial.println("Erro ao tirar foto");
        return;
    }

    // if (mqtt.publish("iot/camera", (const char*)foto->buf, foto->len)) Serial.println("Foto enviada com sucesso");
    // else Serial.println("Falha ao enviar foto");

    snapshot_buf = (uint8_t*)malloc(EI_CAMERA_RAW_FRAME_BUFFER_COLS *
        EI_CAMERA_RAW_FRAME_BUFFER_ROWS * EI_CAMERA_FRAME_BYTE_SIZE
    );
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
    if (EI_CLASSIFIER_INPUT_WIDTH != EI_CAMERA_RAW_FRAME_BUFFER_COLS ||
        EI_CLASSIFIER_INPUT_HEIGHT != EI_CAMERA_RAW_FRAME_BUFFER_ROWS) {
        ei::image::processing::crop_and_interpolate_rgb888(
            snapshot_buf,
            EI_CAMERA_RAW_FRAME_BUFFER_COLS,
            EI_CAMERA_RAW_FRAME_BUFFER_ROWS,
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

    bool person_present = false;

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

        if (bb.value > 0.6f) {
            person_present = true;
        }
    }

    Serial.println(person_present ? "Person found" : "No person");

    free(snapshot_buf);
    // snapshot_buf = nullptr;

    // // ei_camera_get_data fills the EI input buffer from fb->buf
    // // (wire-up depends on your EI library version — see EI's esp32 camera example)
    // numpy::signal_from_buffer((float*)foto->buf, EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT, &signal);

    // ei_impulse_result_t result;
    // run_classifier(&signal, &result, false);

    // bool person_present = false;
    // for (size_t ix = 0; ix < result.bounding_boxes_count; ix++) {
    //     if (result.bounding_boxes[ix].value > 0.6f) { // confidence threshold
    //         person_present = true;
    //         Serial.println("Person found");
    //         // String color = detectShirtColor(foto, result);
    //         // Serial.printf("Person detected! Shirt color: %s (conf: %.2f)\n",
    //         //     color.c_str(), result.bounding_boxes[ix].value);
    //     }
    // }
    // if (!person_present) Serial.println("No person.");

    // esp_camera_fb_return(foto); // libera memória
}

// ── Shirt color detection ──────────────────────────────────────────────
// struct ShirtColor { String name; uint8_t h_min, h_max, s_min; };

// // HSV hue ranges for basic colors
// const ShirtColor COLOR_TABLE[] = {
//     {"Red",    0,  10, 80}, {"Red",   160, 180, 80},
//     {"Orange", 11, 25, 80}, {"Yellow", 26, 34, 80},
//     {"Green",  35, 85, 80}, {"Blue",   86,130, 80},
//     {"Purple",131,159, 80}, {"White",   0,180,  0},  // low saturation
//     {"Black",   0,180,  0},                           // low value
// };

// String detectShirtColor(camera_fb_t* fb, ei_impulse_result_t& result) {
//     // Get bounding box from FOMO result
//     if (result.bounding_boxes[0].value == 0) return "N/A";

//     auto bb = result.bounding_boxes[0];
//     // Crop the lower 40% of the bounding box (torso area)
//     int crop_y = bb.y + (bb.height * 0.55);
//     int crop_h = bb.height * 0.40;

//     // Accumulate HSV hue over the crop region (from RGB565 frame)
//     long h_sum = 0; long s_sum = 0; long v_sum = 0; int count = 0;

//     for (int y = crop_y; y < crop_y + crop_h && y < fb->height; y++) {
//         for (int x = bb.x; x < bb.x + bb.width && x < fb->width; x++) {
//             // RGB565 → R,G,B
//             uint16_t pixel = ((uint16_t*)fb->buf)[y * fb->width + x];
//             uint8_t r = (pixel >> 8) & 0xF8;
//             uint8_t g = (pixel >> 3) & 0xFC;
//             uint8_t b = (pixel << 3) & 0xF8;

//             // RGB → HSV (simplified)
//             uint8_t mx = max({r,g,b}), mn = min({r,g,b});
//             uint8_t delta = mx - mn;
//             float h = 0, s = mx ? (255.0f * delta / mx) : 0, v = mx;
//             if (delta) {
//                 if      (mx==r) h = 43.0f*(g-b)/delta;
//                 else if (mx==g) h = 85.0f + 43.0f*(b-r)/delta;
//                 else            h = 171.0f + 43.0f*(r-g)/delta;
//                 if (h < 0) h += 180;
//             }
//             h_sum += h; s_sum += s; v_sum += v; count++;
//         }
//     }
//     if (!count) return "Unknown";
//     uint8_t ah = h_sum/count, as_ = s_sum/count, av = v_sum/count;

//     if (av < 50)  return "Black";
//     if (as_ < 50) return "White/Gray";
//     for (auto& c : COLOR_TABLE)
//         if (ah >= c.h_min && ah <= c.h_max && as_ >= c.s_min)
//             return c.name;
//     return "Unknown";
// }

// ── Setup & Loop ───────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200); Serial.println("Starting...");

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Erro na câmera: 0x%x\n", err);
        while (true);
    }
    reconectarWiFi();
    conexaoSegura.setCACert(certificado1);
    // mqtt.begin("mqtt.janks.dev.br", 8883, conexaoSegura);
    // mqtt.onMessage(recebeuMensagem);
    // mqtt.setKeepAlive(100);
    // mqtt.setWill("tópico da desconexão", "conteúdo");
    // reconectarMQTT();
}

unsigned long long lastTime = 0;

void loop() {
    reconectarWiFi();
    // reconectarMQTT();
    mqtt.loop(); 

    if (millis() - lastTime >= 1000) {
        tirarFotoEEnviarParaMQTT();
        lastTime = millis();
    }

    // camera_fb_t* fb = esp_camera_fb_get();
    // if (!fb) { Serial.println("Frame capture failed"); return; }

    // esp_camera_fb_return(fb);
    // delay(100);
}