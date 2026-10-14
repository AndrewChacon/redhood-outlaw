#include <driver/i2s.h>

// INMP441 mic -> ESP32 (I2S0, RX)
//   VDD  -> 3V3   (not 5V)
//   GND  -> GND
//   L/R  -> GND   (selects LEFT channel per datasheet; must not be left floating)
//   WS   -> PIN_MIC_WS
//   SCK  -> PIN_MIC_SCK
//   SD   -> PIN_MIC_SD
//
// NOTE: ESP-IDF's I2S RX driver has a known bug that swaps L/R in the
// buffer (espressif/esp-idf#6625). So even with L/R tied to GND, the
// real audio shows up in this code's "R" slot, not "L". That's expected
// here, not a wiring problem -- just read whichever slot has data.
//
// MAX98357A amp -> ESP32 (I2S1, TX), one speaker on its OUT+/OUT- terminals
//   VIN  -> 5V   (3.3V also works, just quieter)
//   GND  -> GND  (shared with mic/ESP32)
//   GAIN -> not connected (floating = default 9dB gain)
//   SD   -> not connected (floating = enabled, outputs (L+R)/2 mix)
//   DIN  -> PIN_AMP_DIN
//   BCLK -> PIN_AMP_BCLK
//   LRC  -> PIN_AMP_LRC
//
// Start with the mic and speaker a few feet apart -- a live mic next to
// a live speaker will squeal with feedback just like a PA system would.

#define I2S_PORT_MIC I2S_NUM_0
#define PIN_MIC_SCK  32
#define PIN_MIC_WS   25
#define PIN_MIC_SD   33

#define I2S_PORT_AMP I2S_NUM_1
#define PIN_AMP_BCLK 26
#define PIN_AMP_LRC  27
#define PIN_AMP_DIN  14

#define SOUND_THRESHOLD 2000  // raw counts; tune after watching the quiet-room baseline

int32_t micBuf[128];  // 64 stereo frames per read
int32_t ampBuf[128];  // rebuilt from micBuf each loop

void setup() {
  Serial.begin(115200);
  delay(1000);

  i2s_config_t micCfg = {
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
  i2s_pin_config_t micPins = {
    .mck_io_num = I2S_PIN_NO_CHANGE,
    .bck_io_num = PIN_MIC_SCK,
    .ws_io_num = PIN_MIC_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = PIN_MIC_SD
  };
  esp_err_t e1 = i2s_driver_install(I2S_PORT_MIC, &micCfg, 0, NULL);
  esp_err_t e2 = i2s_set_pin(I2S_PORT_MIC, &micPins);

  i2s_config_t ampCfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };
  i2s_pin_config_t ampPins = {
    .mck_io_num = I2S_PIN_NO_CHANGE,
    .bck_io_num = PIN_AMP_BCLK,
    .ws_io_num = PIN_AMP_LRC,
    .data_out_num = PIN_AMP_DIN,
    .data_in_num = I2S_PIN_NO_CHANGE
  };
  esp_err_t e3 = i2s_driver_install(I2S_PORT_AMP, &ampCfg, 0, NULL);
  esp_err_t e4 = i2s_set_pin(I2S_PORT_AMP, &ampPins);

  Serial.printf("mic install=%d set_pin=%d | amp install=%d set_pin=%d (0 = OK)\n",
                e1, e2, e3, e4);
}

void loop() {
  size_t bytesRead = 0;
  i2s_read(I2S_PORT_MIC, micBuf, sizeof(micBuf), &bytesRead, portMAX_DELAY);
  int frames = bytesRead / 8;

  // Pull the active mic slot (see RX swap note above) and write it into
  // BOTH amp slots, so it plays back correctly regardless of whether this
  // board's SD pin defaults to mono-mix, left-only, or right-only.
  int32_t peak = 0;
  for (int i = 0; i < frames; i++) {
    int32_t sample = micBuf[i * 2 + 1];
    ampBuf[i * 2]     = sample;
    ampBuf[i * 2 + 1] = sample;
    int32_t level = abs(sample >> 8);
    if (level > peak) peak = level;
  }

  size_t bytesWritten = 0;
  i2s_write(I2S_PORT_AMP, ampBuf, frames * 8, &bytesWritten, portMAX_DELAY);

  static uint32_t lastPrint = 0;
  if (millis() - lastPrint >= 200) {
    lastPrint = millis();
    Serial.printf("peak=%7ld  %s\n", (long)peak, peak > SOUND_THRESHOLD ? "<-- SOUND" : "");
  }
}
