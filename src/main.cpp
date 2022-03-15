#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#define FREQ       10
#define buffer_len 10
volatile int count;
hw_timer_t *timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

#define inputPin GPIO_NUM_26

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 32
Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

static bool above = false;
static int beats = 0;
static int saved_beats = 0;
static int counter = 0;

static int buffer[100] = {0};

int threashold = 3800;
int threashold2 = 1000;

void IRAM_ATTR onTime() {
    portENTER_CRITICAL_ISR(&timerMux);
    count++;
    portEXIT_CRITICAL_ISR(&timerMux);
}

void setup() {
    pinMode(inputPin, INPUT_PULLUP);
    if (!oled.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println(F("SSD1306 allocation failed"));
        while (true)
            ;
    }
    delay(2000);          // wait for initializing
    oled.clearDisplay();  // clear display

    oled.setTextSize(2);       // text size
    oled.setTextColor(WHITE);  // text color
    oled.setCursor(0, 10);     // position to display
    oled.println("Hello :)");  // text to display
    oled.display();            // show on OLED

    Serial.begin(250000);
    timer = timerBegin(0, 80, true);
    timerAttachInterrupt(timer, &onTime, true);

    // Sets an alarm to sound every second
    timerAlarmWrite(timer, 1000000 / FREQ, true);
    timerAlarmEnable(timer);
}

void loop() {
    if (count > 0) {
        // Comment out enter / exit to deactivate the critical section
        portENTER_CRITICAL(&timerMux);
        count--;
        portEXIT_CRITICAL(&timerMux);
        int tempinvalue = analogRead(inputPin);

        for (int i = 100 - 1; i > 0; i--) {
            buffer[i] = buffer[i - 1];
        }
        buffer[0] = tempinvalue;

        if (tempinvalue >= threashold) {
            above = true;
        }
        if (above && tempinvalue < threashold2) {
            above = false;
            beats++;
        }

        if (counter >= FREQ * buffer_len) {
            counter = 0;
            // Serial.println(beats * 6);
            oled.clearDisplay();
            oled.setCursor(0, 10);
            oled.println(beats * 6);
            oled.display();
            saved_beats = beats;
            beats = 0;
        }

        if (counter % (FREQ / 10) == 0) {
            // redraw current bmp
            oled.clearDisplay();
            oled.setCursor(0, 10);
            oled.println(saved_beats * 6);
            Serial.println("-----------------------------");

            // draw signal
            for (int i = 0; i < 100; i++) {
                Serial.println(buffer[i] / 132);
                oled.drawPixel(28 + i, buffer[i] / 132, WHITE);
            }
            oled.display();
        }

        counter++;

        // Serial.println(tempinvalue);
    }
}
