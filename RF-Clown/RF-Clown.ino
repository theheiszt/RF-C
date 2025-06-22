#include "RF24.h"
#include <SPI.h>
#include <ezButton.h>
#include <Adafruit_NeoPixel.h>
#include "esp_bt.h"
#include "esp_wifi.h"

constexpr int BUTTON_PIN    = 34;
constexpr int NEOPIXEL_PIN  = 4;
constexpr int NUM_PIXELS    = 1;
constexpr int SPI_SPEED     = 16000000;

enum class Mode : uint8_t {
    OFF,
    BLE,
    BLUETOOTH,
    ALL,
    MATCH,
    COUNT
};

SPIClass spiVSPI(VSPI);
SPIClass spiHSPI(HSPI);
RF24 radioVSPI(15, 5, SPI_SPEED);
RF24 radioHSPI(22, 21, SPI_SPEED);

Adafruit_NeoPixel pixels(NUM_PIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
const uint8_t bluetooth_channels[] = {32, 34, 46, 48, 50, 52, 0, 1, 2, 4, 6, 8, 22, 24, 26, 28, 30, 74, 76, 78, 80};
const uint8_t ble_channels[] = {2, 26, 80};

Mode currentMode = Mode::OFF;

ezButton modeButton(BUTTON_PIN);

void configureRadio(RF24 &radio, int channel, SPIClass *spi);
void handleModeChange();
void executeMode();
void updateNeoPixel();
void jamBLE();
void jamBluetooth();
void jamAll();
void jamMatched();
uint8_t findBusyChannel();

void setup() {
    Serial.begin(115200);
    
    esp_bt_controller_deinit();
    esp_wifi_stop();
    esp_wifi_deinit();
    esp_wifi_disconnect();
    
    modeButton.setDebounceTime(100);
    pixels.begin();
    pixels.clear();
    pixels.show();
    
    spiVSPI.begin();
    configureRadio(radioVSPI, ble_channels[0], &spiVSPI);

    spiHSPI.begin();
    configureRadio(radioHSPI, bluetooth_channels[0], &spiHSPI);
}

void configureRadio(RF24 &radio, int channel, SPIClass *spi) {
    if (radio.begin(spi)) {
        radio.setAutoAck(false);
        radio.stopListening();
        radio.setRetries(0, 0);
        radio.setPALevel(RF24_PA_MAX, true);
        radio.setDataRate(RF24_2MBPS);
        radio.setCRCLength(RF24_CRC_DISABLED);
        radio.startConstCarrier(RF24_PA_HIGH, channel);
    }
}

void loop() {

    modeButton.loop();
    if (modeButton.isPressed()) {
        handleModeChange();
    }
    executeMode();
}

void handleModeChange() {
    auto next = (static_cast<uint8_t>(currentMode) + 1) % static_cast<uint8_t>(Mode::COUNT);
    currentMode = static_cast<Mode>(next);
    Serial.print("Mode changed to: ");
    Serial.println(static_cast<uint8_t>(currentMode));
    updateNeoPixel();
}

void updateNeoPixel() {
    switch (currentMode) {
        case Mode::OFF:
            pixels.clear();
            pixels.show();
            break;
        case Mode::BLE:
            pixels.setPixelColor(0, pixels.Color(0, 0, 25));
            pixels.show();
            break;
        case Mode::BLUETOOTH:
            pixels.setPixelColor(0, pixels.Color(0, 25, 0));
            pixels.show();
            break;
        case Mode::ALL:
            pixels.setPixelColor(0, pixels.Color(25, 0, 0));
            pixels.show();
            break;
        case Mode::MATCH:
            pixels.setPixelColor(0, pixels.Color(25, 25, 0));
            pixels.show();
            break;
    }
}

void executeMode() {
    switch (currentMode) {
        case Mode::OFF:
            //radioVSPI.powerDown();
            //radioHSPI.powerDown();
            delay(100);
            break;
        case Mode::BLE:
            jamBLE();
            break;
        case Mode::BLUETOOTH:
            jamBluetooth();
            break;
        case Mode::ALL:
            jamAll();
            break;
        case Mode::MATCH:
            jamMatched();
            break;
    }
}

void setRandomChannel(const uint8_t* channels, size_t len) {
    uint8_t channel = channels[random(0, len)];
    radioVSPI.setChannel(channel);
    radioHSPI.setChannel(channel);
}

void jamBLE() {
    setRandomChannel(ble_channels, sizeof(ble_channels));
}

void jamBluetooth() {
    setRandomChannel(bluetooth_channels, sizeof(bluetooth_channels));
}

void jamAll() {
    if (random(0, 2)) {
        jamBluetooth();
    } else {
        jamBLE();
    }
    //delayMicroseconds(20);
}

uint8_t findBusyChannel() {
    for (size_t i = 0; i < sizeof(bluetooth_channels); ++i) {
        uint8_t c = bluetooth_channels[i];
        radioVSPI.setChannel(c);
        delayMicroseconds(110);
        if (radioVSPI.testRPD()) {
            return c;
        }
    }
    for (size_t i = 0; i < sizeof(ble_channels); ++i) {
        uint8_t c = ble_channels[i];
        radioVSPI.setChannel(c);
        delayMicroseconds(110);
        if (radioVSPI.testRPD()) {
            return c;
        }
    }
    return bluetooth_channels[random(0, sizeof(bluetooth_channels))];
}

void jamMatched() {
    uint8_t channel = findBusyChannel();
    radioVSPI.setChannel(channel);
    radioHSPI.setChannel(channel);
}
