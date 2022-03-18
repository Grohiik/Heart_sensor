#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#define FREQ       1000
#define buffer_len 10
volatile int count;
hw_timer_t *timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

#define inputPin GPIO_NUM_26

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 32
Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// For scaling thresholds
static int buffer1[300] = {0};
static uint32_t k = 0;
static float avrage = 1;

float threashold = 3.0 / 2;
float threashold2 = 0.75;

// time between the heartbeats i notice
static int time_between_buffer[buffer_len] = {0};

// used when counting heartbeats and trying to find them
static bool above = false;
static int time_between = 0;
static int saved_beats = 0;
static int beat_counter = 0;

// screen buffer if i need it
static int screen_buffer[100] = {0};

// FILTER
#define M 3
#define N 3

static float xbuff[M + 1] = {0};
static float bettween_buff[N] = {0};
static float ybuff[N] = {0};

// Filter 1 lowpass filter
float filter1_numerator[M + 1] = {0.000003756838019751263726145546276158349,
                                  0.000011270514059253791601953112455625217,
                                  0.000011270514059253791601953112455625217,
                                  0.000003756838019751263726145546276158349};
float filter1_denominator[N] = {2.937170728449890688693812990095466375351,
                                -2.876299723479331049702523159794509410858,
                                0.939098940325282627306080485141137614846};

// Filter 2 highpass filter
float filter2_numerator[M + 1] = {0.994986058442272169877185206132708117366,
                                  -2.984958175326816398609253155882470309734,
                                  2.984958175326816398609253155882470309734,
                                  -0.994986058442272169877185206132708117366};
float filter2_denominator[N] = {2.989946914091736296370527270482853055,
                                -2.979944296951952509289185400120913982391,
                                0.989997256494488331313164053426589816809};

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
    oled.println("Hewo :3");   // text to display
    oled.display();            // show on OLED
    delay(1000);

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
        int invalue = analogRead(inputPin);

        for (int i = M; i > 0; i--) {
            xbuff[i] = xbuff[i - 1];
        }
        xbuff[0] = invalue;

        buffer1[k] = invalue;
        k++;
        if (k == 300) {
            k = 0;

            avrage = 0;
            for (int i = 0; i < 300; i++) {
                avrage += buffer1[i];
            }
            avrage /= 300;
        }

        // FILTERS

        // filter 1
        float outvalue = 0;
        for (int i = 0; i <= M; i++) {
            outvalue += filter1_numerator[i] * xbuff[i];
        }

        for (int i = 0; i < N; i++) {
            outvalue += filter1_denominator[i] * bettween_buff[i];
        }

        for (int i = N - 1; i > 0; i--) {
            bettween_buff[i] = bettween_buff[i - 1];
        }
        bettween_buff[0] = outvalue;

        // filter 2

        outvalue = 0;
        for (int i = 0; i <= M; i++) {
            outvalue += filter2_numerator[i] * bettween_buff[i];
        }

        for (int i = 0; i < N; i++) {
            outvalue += filter2_denominator[i] * ybuff[i];
        }

        for (int i = N - 1; i > 0; i--) {
            ybuff[i] = ybuff[i - 1];
        }
        ybuff[0] = outvalue;

        // found a peak
        if (ybuff[0] >= (threashold * avrage) && time_between > 20) {
            above = true;
        }

        // found a valley after a peak aka a heart beat
        if (above && ybuff[0] < (threashold2 * avrage)) {
            above = false;

            for (int i = buffer_len - 1; i > 0; i--) {
                time_between_buffer[i] = time_between_buffer[i - 1];
            }
            time_between_buffer[0] = time_between;

            time_between = 0;
            beat_counter++;
        }
        // calculate heart bpm after having found buffer_len heart beats
        if (beat_counter >= buffer_len) {
            beat_counter = 0;
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
            oled.display();
            saved_beats = beats;
        }

        // draw signal on screen, %2 to reduce the screen refresh rate, as that
        // caused some problems
        if ((time_between) % 100 == 0) {
            // draw signal
            oled.clearDisplay();
            oled.setCursor(0, 10);
            oled.println(saved_beats);
            for (int i = 0; i < 100; i++) {
                oled.drawPixel(28 + i, 32 - screen_buffer[i] / 132, WHITE);
            }
            oled.display();
        }
        if (time_between % 25 == 0) {
            // screen buffer, in here to make it not be 1kHz
            for (int i = 100 - 1; i > 0; i--) {
                screen_buffer[i] = screen_buffer[i - 1];
            }
            screen_buffer[0] = invalue;
        }

        time_between++;
    }
}
