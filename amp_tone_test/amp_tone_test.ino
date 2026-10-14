#include <driver/i2s.h>
#include <math.h>
#include <string.h>

// Standalone amp/speaker test -- no mic involved. Plays a short, quiet
// beep every couple seconds (not a continuous tone) so it's easy to
// listen for clipping/distortion without it being painful.
//
// MAX98357A amp -> ESP32 (I2S1, TX)
//   VIN  -> VIN (5V)
//   GND  -> GND
//   GAIN -> not connected (floating = default 9dB gain)
//   SD   -> not connected (floating = enabled, outputs (L+R)/2 mix)
//   DIN  -> PIN_AMP_DIN
//   BCLK -> PIN_AMP_BCLK
//   LRC  -> PIN_AMP_LRC

#define I2S_PORT_AMP I2S_NUM_1
#define PIN_AMP_BCLK 26
#define PIN_AMP_LRC  27
#define PIN_AMP_DIN  14

#define SAMPLE_RATE 16000
#define TONE_HZ     440
#define AMPLITUDE   1600000000.0f  // raise toward 2000000000.0f max; back off if it crackles
#define BEEP_MS     150          // length of each beep
#define GAP_MS      1000         // silence between beeps

int32_t ampBuf[128];  // 64 stereo frames per write

void setup() {
  Serial.begin(115200);
  delay(1000);

  i2s_config_t ampCfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = SAMPLE_RATE,
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
  esp_err_t e1 = i2s_driver_install(I2S_PORT_AMP, &ampCfg, 0, NULL);
  esp_err_t e2 = i2s_set_pin(I2S_PORT_AMP, &ampPins);
  Serial.printf("amp install=%d set_pin=%d (0 = OK)\n", e1, e2);
}

void playBeep() {
  const float phaseInc = 2.0f * PI * TONE_HZ / SAMPLE_RATE;
  float phase = 0;
  int framesLeft = SAMPLE_RATE * BEEP_MS / 1000;

  while (framesLeft > 0) {
    int chunk = min(framesLeft, 64);
    for (int i = 0; i < chunk; i++) {
      int32_t sample = (int32_t)(sinf(phase) * AMPLITUDE);
      ampBuf[i * 2]     = sample;
      ampBuf[i * 2 + 1] = sample;
      phase += phaseInc;
      if (phase >= 2.0f * PI) phase -= 2.0f * PI;
    }
    size_t bytesWritten = 0;
    i2s_write(I2S_PORT_AMP, ampBuf, chunk * 8, &bytesWritten, portMAX_DELAY);
    framesLeft -= chunk;
  }
}

void writeSilence(int ms) {
  memset(ampBuf, 0, sizeof(ampBuf));
  int framesLeft = SAMPLE_RATE * ms / 1000;
  while (framesLeft > 0) {
    int chunk = min(framesLeft, 64);
    size_t bytesWritten = 0;
    i2s_write(I2S_PORT_AMP, ampBuf, chunk * 8, &bytesWritten, portMAX_DELAY);
    framesLeft -= chunk;
  }
}

void loop() {
  Serial.println("beep");
  playBeep();
  writeSilence(GAP_MS);
}
