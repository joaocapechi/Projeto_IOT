#include <Arduino.h>
#include <ArduinoJson.h>

// Câmera
#include <esp_camera.h>

// Processamento
#include <lab_human_detection_inferencing.h> // from exported .zip
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
//  .frame_size = FRAMESIZE_QVGA, // (320  x 240) 
 .frame_size = FRAMESIZE_SVGA, // (800 x 600)
//  .frame_size =FRAMESIZE_VGA, // (640 x 480)
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

typedef struct rgb {
    uint8_t R;
    uint8_t G;
    uint8_t B;
} RGB;

RGB mediaCorCamisa(uint8_t *img, int imgW, int imgH, int x, int y, int w, int h) {
    long rSum = 0;
    long gSum = 0;
    long bSum = 0;
    long count = 0;

    int shirtX = x + w * 0.2;
    int shirtY = y + h * 0.3;
    int shirtW = w * 0.6;
    int shirtH = h * 0.35;

    for (int py = shirtY; py < shirtY + shirtH; py++) {
        for (int px = shirtX; px < shirtX + shirtW; px++) {
            if (px < 0 || py < 0 || px >= imgW || py >= imgH) continue;
            
            int idx = (py * imgW + px) * 3;
            rSum += img[idx];
            gSum += img[idx+1];
            bSum += img[idx+2];
            count++;
        }
    }

    RGB color;

    if (count == 0) {
        color.R = color.G = color.B = 0;
    } else {
        color.R = rSum / count;
        color.G = gSum / count;
        color.B = bSum / count;
    }
    return color;
}

String classificarCor(RGB c) {
    uint8_t r = c.R;
    uint8_t g = c.G;
    uint8_t b = c.B;

    if (r < 50 && g < 50 && b < 50) return "black";
    else if (r > 200 && g > 200 && b > 200) return "white";
    else if (abs(r-g) < 20 &&
        abs(r-b) < 20 && abs(g-b) < 20
    ) return "gray";
    else if (r > g + 40 && r > b + 40) return "red";
    else if (g > r + 40 && g > b + 40) return "green";
    else if (b > r + 40 && b > g + 40) return "blue";
    else if (r > 150 && g > 100 && b < 80) return "yellow";
    else return "unknown";
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

    bool person_present = false;
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
            person_present = true;

            coord["x"] = bb.x;
            coord["y"] = bb.y;
            coord["w"] = bb.width;
            coord["h"] = bb.height;
            coord["ei_width"] = EI_CLASSIFIER_INPUT_WIDTH;
            coord["ei_height"] = EI_CLASSIFIER_INPUT_HEIGHT;
            
            RGB corCamisaMedia = mediaCorCamisa(
                snapshot_buf,
                EI_CLASSIFIER_INPUT_WIDTH,
                EI_CLASSIFIER_INPUT_HEIGHT,
                bb.x,
                bb.y,
                bb.width,
                bb.height
            );
            
            String colorName = classificarCor(corCamisaMedia);
            Serial.printf(
                "Camisa: %s RGB(%u, %u, %u)\n",
                colorName.c_str(),
                corCamisaMedia.R,
                corCamisaMedia.G,
                corCamisaMedia.B
            );
            coord["corCamisa_r"] = corCamisaMedia.R;
            coord["corCamisa_g"] = corCamisaMedia.G;
            coord["corCamisa_b"] = corCamisaMedia.B;
            coords.add(coord);
        }
    }

    if (person_present) {
        String string_coords;
        serializeJson(coords, string_coords);
        mqtt.publish("A1/esp32/camera/coord", string_coords);
    }

    Serial.println(person_present ? "Person found" : "No person");

    free(snapshot_buf);
    snapshot_buf = nullptr;
}

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
    mqtt.begin("mqtt.janks.dev.br", 8883, conexaoSegura);
    // mqtt.onMessage(recebeuMensagem);
    mqtt.setKeepAlive(100);
    // mqtt.setWill("tópico da desconexão", "conteúdo");
    reconectarMQTT();
}

unsigned long long lastTime = 0;

void loop() {
    reconectarWiFi();
    reconectarMQTT();
    mqtt.loop(); 

    if (millis() - lastTime >= 3000) {
        tirarFotoEEnviarParaMQTT();
        lastTime = millis();
    }
}