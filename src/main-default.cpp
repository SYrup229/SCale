#include <Arduino.h>
#include <DallasTemperature.h>
#include <OneWire.h>
#include <SD.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <TouchDrvCSTXXX.hpp>
#include <Wire.h>
#include <algorithm>
#include <semphr.h>

// ----- Pin Definitions -----
#define DS18B20_PIN          (2U)
#define SD_CS                (10U)
#define BOARD_I2C_SDA        (18U)
#define BOARD_I2C_SCL        (17U)
#define BOARD_TOUCH_IRQ      (16U)
#define BOARD_TOUCH_RST      (21U)
#ifndef CST816_SLAVE_ADDRESS
  #define CST816_SLAVE_ADDRESS (0x15)
#endif

// ----- Sensor Configuration -----
#define MAX_SENSORS       (5U)
#define MAX_STORED_POINTS (100U)

DeviceAddress sensorAddresses[MAX_SENSORS];
unsigned int sensorCount = 0;
float tempHistories[MAX_SENSORS][MAX_STORED_POINTS] = {0};
unsigned int currentIndex = 0;
uint16_t sensorColors[MAX_SENSORS] = {TFT_GREEN, TFT_RED, TFT_CYAN, TFT_YELLOW, TFT_ORANGE};

// ----- Global Objects -----
TFT_eSPI tft = TFT_eSPI();
TouchDrvCSTXXX touch;
int16_t ts_x[5], ts_y[5];
OneWire oneWire(DS18B20_PIN);
DallasTemperature sensors(&oneWire);

// ----- SD Logging Globals -----
String logFileName = "/File 0.csv";
String logBuffer = "";
bool sdAvailable = false;
TickType_t lastSdCheckTick = 0;
const TickType_t sdCheckIntervalTicks = pdMS_TO_TICKS(10000);

// ----- Home Button & Zoom Globals -----
// Before auto mode starts, the home button triggers auto mode start.
// After auto mode is running, subsequent touches toggle zoom mode.
volatile bool startAuto = false;
volatile bool zoomMode = false;
volatile bool autoStarted = false;

// ------------------- Function Prototypes -------------------
void homeButtonCallback(void* user_data);
void MainTask(void* params);
void TouchTask(void* params);
void initSensors();
void initScreen();
void waitForHomeButtonTouch();

void checkSdConnection();
void writeLogBuffer();
void readAllTemperatures(float* temps);
void logTemperatureData(float* temps);
void autoModeLoop();

// ------------------- Home Button Callback -------------------
void homeButtonCallback(void* user_data)
{
    if (!autoStarted)
    {
        Serial.println("Home button callback: Starting Auto Mode");
        startAuto = true;
    }
    else
    {
        static TickType_t lastToggle = 0;
        TickType_t now = xTaskGetTickCount();
        // Reduced debounce delay to 150 ms.
        if (now - lastToggle >= pdMS_TO_TICKS(150))
        {
            zoomMode = !zoomMode;
            Serial.println("Home button callback: Zoom mode toggled -> " + String(zoomMode ? "ON" : "OFF"));
            lastToggle = now;
        }
    }
}

// ------------------- TouchTask -------------------
// This task polls the touchscreen every 5 ms.
void TouchTask(void* params)
{
    const int homeX = 85;
    const int homeY = 360;
    const int tol = 20;  // Tolerance in pixels
    const TickType_t debounceDelay = pdMS_TO_TICKS(150);
    TickType_t lastTouchTime = 0;

    while (true)
    {
        uint8_t pts = touch.getPoint(ts_x, ts_y, touch.getSupportTouchPoint());
        if (pts > 0)
        {
            for (int i = 0; i < pts; i++)
            {
                int dx = ts_x[i] - homeX;
                int dy = ts_y[i] - homeY;
                if (abs(dx) < tol && abs(dy) < tol)
                {
                    TickType_t now = xTaskGetTickCount();
                    if (now - lastTouchTime >= debounceDelay)
                    {
                        Serial.print("TouchTask: Detected valid touch at (");
                        Serial.print(ts_x[i]);
                        Serial.print(", ");
                        Serial.print(ts_y[i]);
                        Serial.println(")");
                        // Call the callback directly.
                        if (!autoStarted)
                        {
                            Serial.println("TouchTask: Starting Auto Mode");
                            startAuto = true;
                        }
                        else
                        {
                            zoomMode = !zoomMode;
                            Serial.println("TouchTask: Zoom mode toggled -> " + String(zoomMode ? "ON" : "OFF"));
                        }
                        lastTouchTime = now;
                    }
                    break;
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

// ------------------- MainTask -------------------
void MainTask(void* params)
{
    TickType_t lastWakeTime = xTaskGetTickCount();
    const TickType_t loopPeriod = pdMS_TO_TICKS(50);
    while (true)
    {
        // Poll touch events (not strictly needed if TouchTask runs, but can help).
        touch.getPoint(ts_x, ts_y, touch.getSupportTouchPoint());
        checkSdConnection();
        autoModeLoop();
        writeLogBuffer();
        vTaskDelayUntil(&lastWakeTime, loopPeriod);
    }
}

// ------------------- SD & Logging Functions -------------------
void writeLogBuffer()
{
    if (!sdAvailable || logBuffer.length() == 0)
        return;
    File logFile = SD.open(logFileName, FILE_APPEND);
    if (logFile)
    {
        logFile.print(logBuffer);
        logFile.close();
        logBuffer.clear();
    }
    else
    {
        sdAvailable = false;
    }
}

void checkSdConnection()
{
    TickType_t now = xTaskGetTickCount();
    if (!sdAvailable && (now - lastSdCheckTick >= sdCheckIntervalTicks))
    {
        if (SD.begin(SD_CS))
        {
            uint8_t logFileCount = 0;
            while (SD.exists(logFileName))
            {
                logFileCount++;
                logFileName = "/File " + String(logFileCount) + ".csv";
            }
            File file = SD.open(logFileName, FILE_WRITE);
            if (file)
            {
                String header = String(millis());
                for (int i = 0; i < sensorCount; i++)
                {
                    header += ",";
                    for (int j = 0; j < 8; j++)
                    {
                        header += String(sensorAddresses[i][j], HEX);
                    }
                }
                file.println(header);
                file.close();
                sdAvailable = true;
            }
        }
        lastSdCheckTick = now;
    }
}

void readAllTemperatures(float* temps)
{
    sensors.requestTemperatures();
    vTaskDelay(pdMS_TO_TICKS(200)); // Wait for conversion
    for (int i = 0; i < sensorCount; i++)
    {
        temps[i] = sensors.getTempC(sensorAddresses[i]);
    }
}

void logTemperatureData(float* temps)
{
    String logEntry = String(millis());
    for (int i = 0; i < sensorCount; i++)
    {
        logEntry += ",";
        if (temps[i] == DEVICE_DISCONNECTED_C)
            logEntry += "NaN";
        else
            logEntry += String(temps[i], 2);
    }
    logEntry += "\n";
    logBuffer += logEntry;
}

// ------------------- Auto Mode (Graphing and Zoom) -------------------
void autoModeLoop()
{
    static TickType_t lastMeasurementTick = xTaskGetTickCount();
    float temps[MAX_SENSORS] = {0};

    // Layout margins.
    int leftMargin = 40;    // for y-axis scale
    int headerHeight = 30;  // for live sensor header row
    int graphX, graphY, graphWidth, graphHeight;
    int minGraphTemp, maxGraphTemp;

    if (!zoomMode)
    {
        // Normal mode.
        graphX = leftMargin;
        graphY = headerHeight;
        graphWidth = tft.width() - leftMargin - 10;
        graphHeight = tft.height() - headerHeight - 20;
        minGraphTemp = -10;
        maxGraphTemp = 110;
    }
    else
    {
        // Zoom mode: show only the most recent 10 points.
        const int zoomPoints = 10;
        graphX = leftMargin;
        graphY = headerHeight;
        graphWidth = tft.width() - leftMargin - 10;
        graphHeight = tft.height() - headerHeight - 20;
        // Use sensor 1's last reading as reference.
        float ref = tempHistories[0][(currentIndex + MAX_STORED_POINTS - 1) % MAX_STORED_POINTS];
        minGraphTemp = ref - 10;
        maxGraphTemp = ref + 10;
    }

    TickType_t now = xTaskGetTickCount();
    if (now - lastMeasurementTick >= pdMS_TO_TICKS(1000))
    {
        readAllTemperatures(temps);
        if (sdAvailable) { logTemperatureData(temps); }
        for (int i = 0; i < sensorCount; i++)
        {
            tempHistories[i][currentIndex] = temps[i];
        }
        currentIndex = (currentIndex + 1) % MAX_STORED_POINTS;

        // Draw header (live values).
        tft.fillRect(leftMargin, 0, tft.width() - leftMargin, headerHeight, TFT_BLACK);
        int headerY = 5;
        int xPos = leftMargin + 5;
        for (int i = 0; i < sensorCount; i++)
        {
            char valueStr[20];
            sprintf(valueStr, "S%d: %.2f C", i + 1, temps[i]);
            tft.setTextColor(sensorColors[i], TFT_BLACK);
            tft.setTextSize(1);
            tft.drawString(valueStr, xPos, headerY);
            xPos += tft.textWidth(valueStr) + 10;
        }

        // Draw left margin for y-axis scale.
        int calcGraphHeight = tft.height() - headerHeight - 20;
        tft.fillRect(0, headerHeight, leftMargin, calcGraphHeight, TFT_BLACK);

        // Draw graph background.
        tft.fillRect(graphX, headerHeight, graphWidth, calcGraphHeight, TFT_BLACK);
        tft.drawRect(graphX, headerHeight, graphWidth, calcGraphHeight, TFT_WHITE);

        // Draw y-axis scale numbers and grid.
        for (int t = minGraphTemp; t <= maxGraphTemp; t += 10)
        {
            int y = headerHeight + calcGraphHeight - ((t - minGraphTemp) * calcGraphHeight / (maxGraphTemp - minGraphTemp));
            tft.setCursor(0, y - 4);
            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            tft.setTextSize(1);
            tft.printf("%d", t);
            tft.drawLine(graphX, y, graphX + graphWidth, y, TFT_DARKGREY);
        }

        // Draw sensor graphs.
        if (!zoomMode)
        {
            // Normal mode: plot full stored history.
            for (int s = 0; s < sensorCount; s++)
            {
                int prevX = graphX;
                int prevY = headerHeight + calcGraphHeight - ((tempHistories[s][(currentIndex + 1) % MAX_STORED_POINTS] - minGraphTemp) *
                                                              calcGraphHeight / (maxGraphTemp - minGraphTemp));
                for (int i = 1; i < MAX_STORED_POINTS; i++)
                {
                    int idx = (currentIndex + i) % MAX_STORED_POINTS;
                    int x = graphX + (i * graphWidth / MAX_STORED_POINTS);
                    int y = headerHeight + calcGraphHeight - ((tempHistories[s][idx] - minGraphTemp) * calcGraphHeight / (maxGraphTemp - minGraphTemp));
                    y = constrain(y, headerHeight, headerHeight + calcGraphHeight);
                    tft.drawLine(prevX, prevY, x, y, sensorColors[s]);
                    prevX = x;
                    prevY = y;
                }
            }
        }
        else
        {
            // Zoom mode: plot only the most recent 10 points.
            const int N = 10;
            unsigned int startIdx = (currentIndex + MAX_STORED_POINTS - N) % MAX_STORED_POINTS;
            for (int s = 0; s < sensorCount; s++)
            {
                int prevX = graphX;
                int prevY = headerHeight + calcGraphHeight - ((tempHistories[s][startIdx] - minGraphTemp) * calcGraphHeight / (maxGraphTemp - minGraphTemp));
                for (int i = 1; i < N; i++)
                {
                    int idx = (startIdx + i) % MAX_STORED_POINTS;
                    int x = graphX + (i * graphWidth / (N - 1));
                    int y = headerHeight + calcGraphHeight - ((tempHistories[s][idx] - minGraphTemp) * calcGraphHeight / (maxGraphTemp - minGraphTemp));
                    y = constrain(y, headerHeight, headerHeight + calcGraphHeight);
                    tft.drawLine(prevX, prevY, x, y, sensorColors[s]);
                    prevX = x;
                    prevY = y;
                }
            }
        }
        lastMeasurementTick = now;
    }
}


// ------------------- Initialization Functions -------------------
void initSensors()
{
    sensors.begin();
    sensorCount = min((unsigned int)sensors.getDeviceCount(), MAX_SENSORS);
    for (int i = 0; i < sensorCount; i++)
    {
        sensors.getAddress(sensorAddresses[i], i);
    }
    sensors.setResolution(9);
}

void initScreen()
{
    // Initialize the touchscreen reset pin.
    pinMode(BOARD_TOUCH_RST, OUTPUT);
    digitalWrite(BOARD_TOUCH_RST, LOW);
    vTaskDelay(pdMS_TO_TICKS(500));
    digitalWrite(BOARD_TOUCH_RST, HIGH);

    tft.begin();
    tft.setRotation(3);
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_GREEN);
    tft.setTextSize(2);
    touch.setPins(BOARD_TOUCH_RST, BOARD_TOUCH_IRQ);
    touch.begin(Wire, CST816_SLAVE_ADDRESS, BOARD_I2C_SDA, BOARD_I2C_SCL);
    // Set the center button coordinate to define the home button.
    touch.setCenterButtonCoordinate(85, 360);
    touch.disableAutoSleep();
}

void waitForHomeButtonTouch()
{
    Serial.println("Waiting for home button press...");
    tft.fillScreen(TFT_BLACK);
    tft.println("Waiting for home button press...");
    // Set home button callback.
    touch.setHomeButtonCallback(homeButtonCallback, (void*)&startAuto);
    while (!startAuto)
    {
        uint8_t pts = touch.getPoint(ts_x, ts_y, touch.getSupportTouchPoint());
        if (pts > 0)
        {
            for (int i = 0; i < pts; i++)
            {
                int dx = ts_x[i] - 85;
                int dy = ts_y[i] - 360;
                if (abs(dx) < 20 && abs(dy) < 20)
                {
                    startAuto = true;
                    break;
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ------------------- Setup & Loop -------------------
void setup()
{
    Serial.begin(115200);
    Wire.begin(BOARD_I2C_SDA, BOARD_I2C_SCL);
    initSensors();
    initScreen();
    waitForHomeButtonTouch();

    // Clear waiting message and mark auto mode as started.
    tft.fillScreen(TFT_BLACK);
    autoStarted = true;

    // Start MainTask on core 0.
    xTaskCreatePinnedToCore(MainTask, "MainTask", 8192U, nullptr, 1U, nullptr, 0);
    // Start TouchTask on core 1.
    xTaskCreatePinnedToCore(TouchTask, "TouchTask", 4096U, nullptr, 2U, nullptr, 1);
    vTaskDelete(nullptr);
}

void loop()
{
    vTaskDelay(portMAX_DELAY);
}
