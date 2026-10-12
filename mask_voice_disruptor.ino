#include <driver/i2s.h>

// INMP441-style I2S mic -> ESP32
//   VDD  -> 3V3   (not 5V)
//   GND  -> GND
//   L/R  -> GND   (selects LEFT channel per datasheet; must not be left floating)
//   WS   -> PIN_WS
//   SCK  -> PIN_SCK
//   SD   -> PIN_SD
//
// NOTE: ESP-IDF's I2S RX driver has a known bug that swaps L/R in the
// buffer (espressif/esp-idf#6625). So even with L/R tied to GND, the
// real audio shows up in this code's "R" slot, not "L". That's expected
// here, not a wiring problem -- just read whichever slot has data.

#define I2S_PORT I2S_NUM_0
#define PIN_SCK  32
#define PIN_WS   25
#define PIN_SD   33

#define SOUND_THRESHOLD 2000  // raw counts; tune after watching the quiet-room baseline

int32_t buf[128];  // 64 stereo frames per read

void setup() {
  Serial.begin(115200);
  delay(1000);

  i2s_config_t cfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pins = {
    .mck_io_num = I2S_PIN_NO_CHANGE,
    .bck_io_num = PIN_SCK,
    .ws_io_num = PIN_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = PIN_SD
  };

  esp_err_t e1 = i2s_driver_install(I2S_PORT, &cfg, 0, NULL);
  esp_err_t e2 = i2s_set_pin(I2S_PORT, &pins);
  Serial.printf("i2s install=%d set_pin=%d (0 = OK)\n", e1, e2);
}

void loop() {
  size_t bytesRead = 0;
  i2s_read(I2S_PORT, buf, sizeof(buf), &bytesRead, 1000);

  // Each frame = 1 left + 1 right sample, 4 bytes each = 8 bytes/frame.
  // Only one slot carries real audio (the other stays near zero) --
  // printing both makes it obvious which one is active without caring
  // about the L/R swap bug noted above.
  int frames = bytesRead / 8;
  int32_t peakL = 0, peakR = 0;
  for (int i = 0; i < frames; i++) {
    int32_t l = abs(buf[i * 2] >> 8);
    int32_t r = abs(buf[i * 2 + 1] >> 8);
    if (l > peakL) peakL = l;
    if (r > peakR) peakR = r;
  }

  bool soundDetected = (peakL > SOUND_THRESHOLD) || (peakR > SOUND_THRESHOLD);
  Serial.printf("L peak=%7ld  R peak=%7ld  %s\n",
                (long)peakL, (long)peakR,
                soundDetected ? "<-- SOUND" : "");
  delay(200);
}
