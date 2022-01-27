#include <Arduino.h>

#ifndef UNIT_TEST

#include <NTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

#include <Adafruit_NeoPixel.h>

#include <DNSServer.h>        //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h> //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>

#include <SPI.h>
#include <RTClib.h>
#include <Wire.h>

//
// Connect a 4x6 matrix to D1 pin
// the first 4 leds will be the ones of seconds and the second four the
// tens of seconds and so on
//
//                 L5 - L4
//                 |    |
//                 L6   L3
//            etc  |    |
//            L1   L7   L2
//            |    |    |
//            L9 - L8   L1
//

#define PIN D3
#define PIXELS 24 // 6 by 4 array

WiFiManager wifiManager;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "ntp1.aliyun.com", 60 * 60 * 8, 60000);
Adafruit_NeoPixel strip = Adafruit_NeoPixel(PIXELS, PIN, NEO_GRB + NEO_KHZ400);

RTC_DS3231 rtc;
char t[32];
volatile bool tick;

void inline timer0_ISR(void)
{
    tick = true;
    // reprime the timer
    timer0_write(ESP.getCycleCount() + 80000000L); // 80MHz == 1sec
}

void show(uint8_t offset, int t)
{
    // offset 0 for seconds
    // offset 8 for minutes
    // offset 12 for hours
    uint8_t tens = t / 10;
    uint8_t ones = t % 10;

    // ones go up
    for (uint8_t i = 0; i < 4; i++)
    {
        if (bitRead(ones, i))
        {
            strip.setPixelColor(offset + 7 - i, strip.Color(10, 22, 22));
        }
    }

    // tens go down
    for (uint8_t i = 0; i < 4; i++)
    {
        if (bitRead(tens, i))
        {
            strip.setPixelColor(offset + i, strip.Color(10, 22, 22));
        }
    }
}

void show_test_led(int count)
{
    strip.clear();
    show(8, count);
    strip.show();
}

void init_network()
{
    Serial.print("init network...");
    wifiManager.setTimeout(600);
    bool connect = wifiManager.autoConnect();
    Serial.println("Connecting to wifi");
    if (!connect)
    {
        delay(1000);
    }
    else
    {
        show_test_led(4);
        timeClient.update();
        if (timeClient.getEpochTime() < 1643308074)
        {
            init_network();
        }
        DateTime ntpTime = DateTime(timeClient.getEpochTime());
        Serial.print(timeClient.getEpochTime());
        Serial.print(F(" => NTP DateTime: "));
        Serial.println(ntpTime.toString("YYYY/MM/DD hh:mm:ss"));
        Serial.println("Set current time...");
        rtc.adjust(ntpTime);
    }
    show_test_led(8);
}

void setup()
{
    // start serial so we can easily debug through it
    Serial.begin(115200);

    // ----------------
    Wire.begin();
    rtc.begin();
    // ----------------

    // start and clear the strip
    strip.begin();
    strip.clear();
    strip.show();

    // start wifi manager, it will either make connection
    // or open ap for saving the network config
    // wifiManager.autoConnect();

    // start ntp client
    timeClient.begin();

    // connect timer interrupt
    noInterrupts();
    timer0_isr_init();
    timer0_attachInterrupt(timer0_ISR);
    timer0_write(ESP.getCycleCount() + 80000000L); // 80MHZ / 1sec
    interrupts();
    init_network();
    // prime our first tick
    tick = true;
}

void loop()
{
    // update on every tick
    if (tick)
    {
        Serial.print(F("Temperature: "));
        Serial.print(rtc.getTemperature());
        Serial.print(F("C°, DS3231 DateTime: "));
        Serial.println(rtc.now().toString("YYYY/MM/DD hh:mm:ss"));

        // update timeclient
        // timeClient.update();
        // debug output to serial
        // Serial.println(timeClient.getFormattedTime());

        // clear the strip
        strip.clear();

        Serial.println(rtc.now().second());

        show(16, rtc.now().second());
        show(8, rtc.now().minute());
        show(0, rtc.now().hour());

        // push to the strip
        strip.show();

        // wait for next tick
        tick = false;
    }
}

#endif