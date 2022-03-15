#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#define FREQ       20
#define buffer_len 5
volatile int count;
hw_timer_t *timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

#define inputPin GPIO_NUM_26

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 32
Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

static int time_between_buffer[buffer_len] = {0};

static bool above = false;
static int saved_beats = 0;
static int time_between = 0;
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
    Serial.begin(250000);
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

        // found a peak
        if (tempinvalue >= threashold) {
            above = true;
        }
        // found a valley after a peak aka a heart beat
        if (above && tempinvalue < threashold2) {
            above = false;

            for (int i = buffer_len - 1; i > 0; i--) {
                time_between_buffer[i] = time_between_buffer[i - 1];
            }
            time_between_buffer[0] = time_between;

            time_between = 0;
            counter++;
        }
        // calculate heart bpm after having found buffer_len heart beats
        if (counter >= buffer_len) {
            counter = 0;
            float sum = 0;
            for (int i = 0; i < buffer_len; i++) {
                sum += time_between_buffer[i];
            }
            sum /= (float)(buffer_len * FREQ);

            int beats = (int)(60.0 / sum);
            // Serial.println(beats);
            oled.clearDisplay();
            oled.setCursor(0, 10);
            oled.println(beats);
            saved_beats = beats;
        }

        time_between++;

        // draw signal on screen, %2 to reduce the screen refresh rate, as that
        // caused some problems
        if (time_between % 2) {
            // draw signal
            oled.clearDisplay();
            oled.setCursor(0, 10);
            oled.println(saved_beats);
            for (int i = 0; i < 100; i++) {
                oled.drawPixel(28 + i, buffer[i] / 132, WHITE);
            }
            oled.display();
        }
    }
}
