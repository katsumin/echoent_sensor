#include <M5Unified.h>
#include "EL.h"
#include <DFRobot_SCD4X.h>

#define WIFI_SSID "ssid" // !!!! change
#define WIFI_PASS "pass" // !!!! change
#define NTP_TIMEZONE "JST-9"
#define NTP_SERVER1 "ntp.nict.jp"
#define NTP_SERVER2 "ntp.jst.mfeed.ad.jp"
#define NTP_SERVER3 ""

WiFiClient client;
WiFiUDP elUDP;
byte eojs[][3] = {
    {0x00, 0x11, 0x01}, // 温度センサ
    {0x00, 0x12, 0x01}, // 湿度センサ
    {0x00, 0x1b, 0x01}, // CO2センサ
};
int devices = sizeof(eojs) / sizeof(byte[3]);
EL echo(elUDP, eojs, devices);

DFRobot_SCD4X SCD4X(&Wire, /*i2cAddr = */ SCD4X_I2C_ADDR);

#define IP_BUF_LEN (15 + 1)
char *buf_ip = new char[IP_BUF_LEN]; // 192.168.100.100
void printNetData()
{
    Serial.println("---");

    // IP
    // print your WiFi shield's IP address:
    IPAddress ip = WiFi.localIP();
    Serial.print("IP  Address: ");
    Serial.println(ip);
    snprintf(buf_ip, IP_BUF_LEN, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);

    IPAddress dgwip = WiFi.gatewayIP();
    Serial.print("DGW Address: ");
    Serial.println(dgwip);

    IPAddress smip = WiFi.subnetMask();
    Serial.print("SM  Address: ");
    Serial.println(smip);

    Serial.println("---");
}

// NTP同期
void syncNtp()
{
    configTzTime(NTP_TIMEZONE, NTP_SERVER1, NTP_SERVER2, NTP_SERVER3);
    while (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED)
    {
        Serial.print('.');
        delay(1000);
    }
    Serial.println("\r\n NTP Connected.");

    time_t t = time(nullptr) + 1; // Advance one second.
    while (t > time(nullptr))
        ; /// Synchronization in seconds
    M5.Rtc.setDateTime(localtime(&t));
}

void dispButton(const char *label, int x, bool state_on)
{
    M5.Lcd.setFont(&fonts::Font0);
    if (state_on)
        M5.Lcd.setTextColor(TFT_MAGENTA);
    else
        M5.Lcd.setTextColor(TFT_WHITE);
    int y = 0;
    int w = M5.Lcd.textWidth(label);
    int h = M5.Lcd.fontHeight();
    int button_width = w + 4;
    int button_height = h + 4;
    M5.Lcd.drawCenterString(label, x, y + 2);
    M5.Lcd.drawRoundRect(x - button_width / 2, y, button_width, button_height, 2, TFT_WHITE);
}

int32_t width;
int32_t height;
int y_pos_co2 = 15;
int y_pos_temp = 33;
int y_pos_humidity = 51;
#define MEAS_BUF_LEN (6 + 1)
char *buf_meas = new char[MEAS_BUF_LEN]; // xxx.xx
void dispMeasurement(bool value, float t = 0, float h = 0, uint16_t co2 = 0)
{
    M5.Lcd.setFont(&fonts::lgfxJapanGothic_16);
    if (value)
    {
        int32_t txt_width = M5.Lcd.textWidth("xxx.xx");
        int32_t unit_width = M5.Lcd.textWidth(" ppm");
        int32_t txt_height = M5.Lcd.fontHeight(&fonts::lgfxJapanGothic_16);
        M5.Lcd.fillRect(width - txt_width - unit_width, y_pos_co2, txt_width, txt_height, TFT_BLACK);
        M5.Lcd.fillRect(width - txt_width - unit_width, y_pos_temp, txt_width, txt_height, TFT_BLACK);
        M5.Lcd.fillRect(width - txt_width - unit_width, y_pos_humidity, txt_width, txt_height, TFT_BLACK);

        M5.Lcd.setTextColor(TFT_WHITE);
        snprintf(buf_meas, MEAS_BUF_LEN, "%5.2f", t);
        M5.Lcd.drawRightString(buf_meas, width - unit_width, y_pos_temp);
        snprintf(buf_meas, MEAS_BUF_LEN, "%5.2f", h);
        M5.Lcd.drawRightString(buf_meas, width - unit_width, y_pos_humidity);
        int c = TFT_WHITE;
        if (co2 >= 1500)
            c = TFT_RED;
        else if (co2 >= 1000)
            c = TFT_YELLOW;
        snprintf(buf_meas, MEAS_BUF_LEN, "%5d", co2);
        M5.Lcd.setTextColor(c);
        M5.Lcd.drawRightString(buf_meas, width - unit_width, y_pos_co2);
    }
    else
    {
        M5.Lcd.setTextColor(TFT_WHITE);
        M5.Lcd.drawString("CO2:", 1, y_pos_co2);
        M5.Lcd.drawRightString("ppm", width - 1, y_pos_co2);
        M5.Lcd.drawString("温度:", 1, y_pos_temp);
        M5.Lcd.drawRightString("℃", width - 1, y_pos_temp);
        M5.Lcd.drawString("湿度:", 1, y_pos_humidity);
        M5.Lcd.drawRightString("%RH", width - 1, y_pos_humidity);
    }
}

void setup()
{
    M5.begin();

    Serial.begin(115200);

    // LCD
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setRotation(1);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setFont(&fonts::Font0);
    width = M5.Lcd.width();
    height = M5.Lcd.height();

    // Speaker
    M5.Speaker.begin();
    M5.Speaker.setVolume(64);
    if (M5.Speaker.isEnabled())
    {
        Serial.println("speaker enabled.");
    }

    // RTC
    for (;;)
    {
        if (!M5.Rtc.isEnabled())
            Serial.println("RTC not found.");
        else
            break;
        vTaskDelay(500);
    }
    Serial.println("RTC found.");

    while (!SCD4X.begin())
    {
        Serial.println("Communication with device failed, please check connection");
        delay(3000);
    }
    Serial.println("Begin ok!");
    SCD4X.enablePeriodMeasure(SCD4X_START_PERIODIC_MEASURE);

    M5.Lcd.println("wifi connect start");

    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("wait...");
        delay(1000);
    }
    M5.Lcd.println("wifi connect ok");

    // NTP & RTC
    syncNtp();

    M5.update();

    printNetData();
    echo.begin(); // EL 起動シーケンス

    echo.profile[0x80] = new byte[1 + 1]{1, 0x30};                                                                                                   // power
    echo.profile[0x81] = new byte[1 + 1]{1, 0x40};                                                                                                   // position
    echo.profile[0x82] = new byte[4 + 1]{4, 0x00, 0x00, 0x4b, 0x00};                                                                                 // release K
    echo.profile[0x83] = new byte[17 + 1]{17, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04}; // identification number
    echo.profile[0x8a] = new byte[3 + 1]{3, 0x00, 0x00, 0x00};                                                                                       // maker unknown

    echo.devices[0][0x80] = echo.profile[0x80];                                                                   // power
    echo.devices[0][0x81] = echo.profile[0x81];                                                                   // position
    echo.devices[0][0x82] = echo.profile[0x82];                                                                   // release K
    echo.devices[0][0x83] = echo.profile[0x83];                                                                   // identification number
    echo.devices[0][0x88] = new byte[1 + 1]{1, 0x42};                                                             // error status
    echo.devices[0][0x8a] = echo.profile[0x8a];                                                                   // maker unknown
    echo.devices[0][0xe0] = new byte[2 + 1]{2, 0x00, 0x00};                                                       // temperature
    echo.devices[0][0x9D] = new byte[2 + 1]{2, 0x01, 0x80};                                                       // 状変アナウンスプロパティ
    echo.devices[0][0x9E] = new byte[2 + 1]{2, 0x01, 0x81};                                                       // Setプロパティマップ
    echo.devices[0][0x9f] = new byte[11 + 1]{11, 10, 0x80, 0x81, 0x82, 0x83, 0x88, 0x8a, 0xe0, 0x9d, 0x9e, 0x9f}; // Getプロパティマップ

    echo.devices[1][0x80] = echo.profile[0x80];                                                                   // power
    echo.devices[1][0x81] = echo.profile[0x81];                                                                   // position
    echo.devices[1][0x82] = echo.profile[0x82];                                                                   // release K
    echo.devices[1][0x83] = echo.profile[0x83];                                                                   // identification number
    echo.devices[1][0x88] = new byte[1 + 1]{1, 0x42};                                                             // error status
    echo.devices[1][0x8a] = echo.profile[0x8a];                                                                   // maker unknown
    echo.devices[1][0xe0] = new byte[1 + 1]{1, 0x00};                                                             // humidity
    echo.devices[1][0x9D] = new byte[2 + 1]{2, 0x01, 0x80};                                                       // 状変アナウンスプロパティ
    echo.devices[1][0x9E] = new byte[2 + 1]{2, 0x01, 0x81};                                                       // Setプロパティマップ
    echo.devices[1][0x9f] = new byte[11 + 1]{11, 10, 0x80, 0x81, 0x82, 0x83, 0x88, 0x8a, 0xe0, 0x9d, 0x9e, 0x9f}; // Getプロパティマップ

    echo.devices[2][0x80] = echo.profile[0x80];                                                                   // power
    echo.devices[2][0x81] = echo.profile[0x81];                                                                   // position
    echo.devices[2][0x82] = echo.profile[0x82];                                                                   // release K
    echo.devices[2][0x83] = echo.profile[0x83];                                                                   // identification number
    echo.devices[2][0x88] = new byte[1 + 1]{1, 0x42};                                                             // error status
    echo.devices[2][0x8a] = echo.profile[0x8a];                                                                   // maker unknown
    echo.devices[2][0xe0] = new byte[2 + 1]{2, 0x00, 0x00};                                                       // co2
    echo.devices[2][0x9D] = new byte[2 + 1]{2, 0x01, 0x80};                                                       // 状変アナウンスプロパティ
    echo.devices[2][0x9E] = new byte[2 + 1]{2, 0x01, 0x81};                                                       // Setプロパティマップ
    echo.devices[2][0x9f] = new byte[11 + 1]{11, 10, 0x80, 0x81, 0x82, 0x83, 0x88, 0x8a, 0xe0, 0x9d, 0x9e, 0x9f}; // Getプロパティマップ

    for (int i = 0; i < devices; i++)
    {
        echo.devices[i].printAll();
    }

    const byte seoj[] = {0x0e, 0xf0, 0x01};
    const byte deoj[] = {0x0e, 0xf0, 0x01};
    echo.sendMultiOPC1(seoj, deoj, EL_INF, 0xd5, echo.profile[0xd5]);

    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextColor(TFT_YELLOW);
    M5.Lcd.setFont(&fonts::Font0);
    M5.Lcd.drawString(buf_ip, 1, 0);

    dispMeasurement(false);

    dispButton("NTP", 100, false);
}

int getDeviceId(byte eoj0, byte eoj1, byte eoj2)
{
    const int count = sizeof(eojs) / sizeof(byte[3]);
    int id = -1;
    for (int i = 0; i < count; i++)
    {
        if (eoj0 == eojs[i][0] && eoj1 == eojs[i][1] && eoj2 == eojs[i][2])
        {
            id = i;
            break;
        }
    }

    Serial.printf("devId:%d, eoj:%02x%02x%02x", id, eoj0, eoj1, eoj2);
    Serial.println();
    return id;
}

int count = 0;
byte *buf_t = new byte[3]{
    2,
    0,
    0,
};
byte *buf_h = new byte[2]{
    1,
    0,
};
byte *buf_co2 = new byte[3]{
    2,
    0,
    0,
};
#define DATE_BUF_LEN (19 + 1)
char *buf_rtc = new char[DATE_BUF_LEN]; // 2023/01/01 17:57:08
void loop()
{
    if (M5.BtnB.wasReleased())
    {
        dispButton("NTP", 100, true);
        syncNtp();
        dispButton("NTP", 100, false);
    }
    if (count % 5 == 0)
    {
        auto dt = M5.Rtc.getDateTime();
        snprintf(buf_rtc, DATE_BUF_LEN, "%04d/%02d/%02d %02d:%02d:%02d",
                 dt.date.year, dt.date.month, dt.date.date,
                 dt.time.hours, dt.time.minutes, dt.time.seconds);
        Serial.println(buf_rtc);

        // M5.Lcd.fillScreen(BLACK);
        M5.Lcd.setTextColor(TFT_YELLOW);
        M5.Lcd.setFont(&fonts::Font0);
        int32_t txt_width = M5.Lcd.textWidth(buf_rtc);
        int32_t txt_height = M5.Lcd.fontHeight(&fonts::Font0);
        M5.Lcd.fillRect(width - txt_width, height - txt_height, txt_width, txt_height, TFT_BLACK);
        M5.Lcd.drawRightString(buf_rtc, width - 1, height - txt_height);

        if (dt.time.hours == 0 && dt.time.minutes == 0 && dt.time.seconds == 0)
            // 毎日0時にNTP同期
            syncNtp();
    }

    if (SCD4X.getDataReadyStatus())
    {
        DFRobot_SCD4X::sSensorMeasurement_t data;
        SCD4X.readMeasurement(&data);

        Serial.print("Carbon dioxide concentration : ");
        uint16_t co2 = data.CO2ppm;
        Serial.print(co2);
        Serial.println(" ppm");
        buf_co2[1] = co2 >> 8;
        buf_co2[2] = co2 & 0xff;
        echo.update(2, 0xe0, buf_co2); // ECHONET Liteの状態を変更

        Serial.print("Environment temperature : ");
        float t = data.temp;
        Serial.print(t);
        Serial.println(" C");
        uint16_t s_t = 0x7fff;
        if (t < -273.2)
            s_t = 0x8000;
        else if (t <= 3276.6)
            s_t = (uint16_t)(t * 10);
        buf_t[1] = s_t >> 8;
        buf_t[2] = s_t & 0xff;
        echo.update(0, 0xe0, buf_t); // ECHONET Liteの状態を変更

        Serial.print("Relative humidity : ");
        float h = data.humidity;
        Serial.print(h);
        Serial.println(" RH");
        buf_h[1] = (byte)h;
        echo.update(1, 0xe0, buf_h); // ECHONET Liteの状態を変更

        M5.Speaker.tone(1000, 200);

        // 測定値表示更新
        dispMeasurement(true, t, h, co2);
    }

    // パケット貰ったらやる
    int packetSize = 0; // 受信データ量

    if (0 != (packetSize = echo.read())) // 0!=はなくてもよいが，Warning出るのでつけとく
    {                                    // 受け取った内容読み取り，あったら中へ
        // ESVがSETとかGETとかで動作をかえる
        switch (echo._rBuffer[EL_ESV])
        {
        case EL_SETI:
        case EL_SETC:
        {
            const int devId = getDeviceId(echo._rBuffer[EL_DEOJ + 0], echo._rBuffer[EL_DEOJ + 1], echo._rBuffer[EL_DEOJ + 2]);
            if (devId >= 0)
            {
                switch (echo._rBuffer[EL_EPC])
                {
                case 0x81:
                    byte *pdcedt = new byte[2]{0x01, echo._rBuffer[EL_EDT + 0]}; // ECHONET Liteの状態を変更
                    echo.update(devId, echo._rBuffer[EL_EPC], pdcedt);           // ECHONET Liteの状態を変更
                    delete[] pdcedt;
                    break;
                }
            }

            if (echo._rBuffer[EL_ESV] == EL_SETC)
            { // SETCなら返信必要
                echo.returner();
            }
        }
        break;
        case EL_GET:
            echo.returner();
            break; // GetとINF_REQここまで
        }
    }
    delay(200);

    count = (count + 1) % 10;
    M5.update();
}