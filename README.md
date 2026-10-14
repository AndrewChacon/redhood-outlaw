# redhood-outlaw

Red Hood Outlaws cosplay mask build: red LED eyes, an I2S voice disruptor
(mic + amp + speaker), and an ESP32-hosted web app for WiFi/Bluetooth
network scanning.

## Board

ESP32 Dev Module -- 30-pin ESP32-WROOM-32 DevKitC-style board (CP2102
USB-UART, micro USB).

## Wiring

### INMP441 mic -> ESP32 (I2S0, RX)

| Mic pin | ESP32 pin |
|---|---|
| VDD | 3V3 |
| GND | GND |
| L/R | GND |
| WS  | GPIO25 |
| SCK | GPIO32 |
| SD  | GPIO33 |

### MAX98357A amp -> ESP32 (I2S1, TX)

| Amp pin | ESP32 pin |
|---|---|
| VIN  | VIN (5V) |
| GND  | GND |
| GAIN | not connected |
| SD   | not connected |
| DIN  | GPIO14 |
| BCLK | GPIO26 |
| LRC  | GPIO27 |

### Speaker -> amp screw terminal

| Speaker wire | Amp terminal |
|---|---|
| Red (+) | OUT+ |
| Black (-) | OUT- |

### Notes

- ESP-IDF's I2S RX driver swaps L/R in the buffer
  (espressif/esp-idf#6625) -- real mic audio lands in the "R" slot even
  with L/R tied to GND. Expected, not a wiring bug.
- Keep mic and speaker physically apart to avoid acoustic feedback.
- Amp VIN shares the board's 5V/VIN rail with everything else -- add a
  bulk capacitor (100-470uF) across the amp's VIN/GND if the mic acts up
  when the amp is driving the speaker.
