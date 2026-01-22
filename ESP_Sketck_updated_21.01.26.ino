/*
 * ESP32 GM77 Barcode Scanner with Robridge Integration
 * 
 * This code combines:
 * - GM77 barcode scanner functionality
 * - OLED display with status information
 * - Gemini AI analysis
 * - Robridge web application integration
 * 
 * Hardware Requirements:
 * - ESP32 board
 * - GM77 barcode scanner (UART2: GPIO16 RX, GPIO17 TX)
 * - SH1106 OLED display (I2C: 0x3C)
 * - WiFi connection
 * 
 * Setup:
 * 1. Update WiFi credentials below
 * 2. Update server IP address to your computer's IP
 * 3. Upload this code to ESP32
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>   // Use SH1106/SH1107 driver
#include <WiFi.h>
#include <WiFiManager.h>          // <- ADD THIS LINE
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <NetworkClientSecure.h>
#include <ArduinoJson.h>
#include <Preferences.h>         // For storing server config
#include "MAX1704X.h"            // MAX1704X Fuel Gauge Library

// ---------------------------------------------------------------
// Light Sleep Integration Configuration  
// ---------------------------------------------------------------
#define SLEEP_TIMEOUT 100000  // Enter light sleep after 10 seconds of inactivity

// ---------------------------------------------------------------
// GM77 Trigger Pin Configuration
// ---------------------------------------------------------------
#define GM77_TRIG_PIN 35  // GM77 trigger button pin for wake-on-trigger

unsigned long lastActivityTime = 0;
unsigned long sleepStartTime = 0;
bool displayOn = true;
String lastBarcode = "";


// --- WiFi Configuration ---
const char* ssid = "Sasikumar ";
const char* password = "45061300";

// --- Robridge Server Configuration ---
//String expressServerURL = "http://10.204.193.1:3001";  // Express backend - LOCAL
//String aiServerURL = "http://10.204.193.1:8000";  // AI server - LOCAL
String expressServerURL = "https://robridge-express-zl9j.onrender.com";  // Express backend - LOCAL
String aiServerURL = "https://robridge-ai-tgc9.onrender.com";  // AI server - Render deployed
String customServerIP = "";  // Custom server IP from portal

// --- ESP32 Device Configuration ---
const String deviceId = "Robridge_Scanner_03";
const String deviceName = "Robridge Scanner 03";  // ✅ AI ENABLED - Contains "AI"
const String firmwareVersion = "2.0.0";

// --- Gemini API Configuration ---
const char* gemini_api_key = "AIzaSyASPgBz59it8cF3biu1q75RtuDesEeJc1M";
const char* gemini_api_url = "https://generativelanguage.googleapis.com/v1beta/models/gemini-pro:generateContent";

// --- OLED Setup ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);  // for SH1106

// --- GM77 Barcode Scanner on Serial2 ---
HardwareSerial GM77(2); // UART2 on ESP32 (GPIO16 RX, GPIO17 TX)

// --- Product Structure ---
struct Product {
    String barcode;
    String name;
    String type;
    String details;
    String price;
    String category;
    String location;
};


// Robridge Logo bitmap data (Working Version)
static const unsigned char PROGMEM epd_bitmap_ro_bridge[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x0f, 0xe0, 0x00, 0x00, 0xff, 0xc0, 0x00, 0x00, 0x78, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x0f, 0xf8, 0x00, 0x00, 0xff, 0xf8, 0x00, 0x00, 0x78, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x0f, 0xfc, 0x00, 0x00, 0xff, 0x3c, 0x00, 0x00, 0x78, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x0f, 0xbe, 0x00, 0x00, 0xff, 0x3e, 0x00, 0x00, 0x78, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x0f, 0xbe, 0x00, 0x00, 0xfe, 0x1e, 0x00, 0x00, 0x30, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x0f, 0x9f, 0x00, 0x00, 0xfe, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x0f, 0x9f, 0x00, 0x00, 0x06, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x0f, 0x1f, 0x00, 0x00, 0x06, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x0f, 0x1f, 0x80, 0x00, 0xfe, 0xff, 0x00, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x0e, 0x0f, 0x80, 0x00, 0xfc, 0xff, 0x00, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x0e, 0x0f, 0x80, 0x00, 0xfd, 0xff, 0x00, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x0e, 0x0f, 0x80, 0xf0, 0x01, 0xff, 0x06, 0x00, 0x70, 0x07, 0x0e, 0x00, 0x60, 0x00, 0x1c, 0x00, 
	0x0e, 0x07, 0x83, 0xfc, 0x03, 0xff, 0x07, 0x3c, 0x70, 0x1f, 0xce, 0x01, 0xf9, 0xc0, 0x7f, 0x00, 
	0x0e, 0x0f, 0x83, 0xfc, 0x7f, 0xff, 0x07, 0x7c, 0x70, 0x1f, 0xce, 0x03, 0xf9, 0xc0, 0x7f, 0x80, 
	0x0e, 0x8f, 0x87, 0xfe, 0x7f, 0xff, 0x07, 0x78, 0x70, 0x3f, 0xee, 0x07, 0xfd, 0xc0, 0xff, 0xc0, 
	0x0f, 0x2f, 0x8f, 0xff, 0x7f, 0xff, 0x07, 0xf8, 0x70, 0x3c, 0xfe, 0x07, 0x9d, 0xc1, 0xe3, 0xc0, 
	0x0f, 0xff, 0x0f, 0x0f, 0x3f, 0x9f, 0x07, 0xf8, 0x70, 0x78, 0x3e, 0x0f, 0x07, 0xc1, 0xc1, 0xe0, 
	0x0e, 0x0f, 0x1e, 0x07, 0x3f, 0x8e, 0x07, 0xc0, 0x70, 0x70, 0x1e, 0x0e, 0x07, 0xc3, 0x80, 0xe0, 
	0x0e, 0x0f, 0x1e, 0x07, 0xbf, 0x8e, 0x07, 0xc0, 0x70, 0x70, 0x1e, 0x1e, 0x03, 0xc3, 0x80, 0xe0, 
	0x0f, 0x0f, 0x1c, 0x03, 0x9f, 0x04, 0x07, 0x80, 0x70, 0xe0, 0x1e, 0x1c, 0x03, 0xc3, 0x80, 0x60, 
	0x0f, 0x1e, 0x1c, 0x03, 0x80, 0x06, 0x07, 0x00, 0x70, 0xe0, 0x0e, 0x1c, 0x01, 0xc3, 0x00, 0x70, 
	0x0f, 0x1e, 0x3c, 0x03, 0x9f, 0x0f, 0x07, 0x00, 0x70, 0xe0, 0x0e, 0x1c, 0x01, 0xc7, 0x00, 0x70, 
	0x0f, 0xfc, 0x38, 0x01, 0xdf, 0x8f, 0x07, 0x00, 0x70, 0xe0, 0x0e, 0x18, 0x01, 0xc7, 0x00, 0x70, 
	0x0f, 0x1c, 0x38, 0x01, 0xdf, 0x8f, 0x07, 0x00, 0x70, 0xe0, 0x0e, 0x18, 0x01, 0xc7, 0x00, 0x70, 
	0x0f, 0x9c, 0x38, 0x01, 0xdf, 0xff, 0x87, 0x00, 0x70, 0xe0, 0x0e, 0x18, 0x01, 0xc7, 0xff, 0xf0, 
	0x0f, 0x9c, 0x38, 0x01, 0xdf, 0xff, 0x87, 0x00, 0x70, 0xe0, 0x0e, 0x18, 0x01, 0xc7, 0xff, 0xf0, 
	0x0f, 0x9c, 0x38, 0x01, 0xdf, 0xff, 0x87, 0x00, 0x70, 0xe0, 0x0e, 0x18, 0x01, 0xc7, 0xff, 0xf0, 
	0x0f, 0x9c, 0x38, 0x01, 0xdf, 0xff, 0x87, 0x00, 0x70, 0xe0, 0x0e, 0x18, 0x01, 0xc7, 0x00, 0x00, 
	0x0f, 0x9e, 0x38, 0x01, 0x83, 0xff, 0x87, 0x00, 0x70, 0xe0, 0x0e, 0x1c, 0x01, 0xc7, 0x00, 0x00, 
	0x0f, 0x9e, 0x3c, 0x03, 0x81, 0xff, 0x87, 0x00, 0x70, 0xe0, 0x0e, 0x1c, 0x01, 0xc7, 0x00, 0x00, 
	0x0f, 0x9e, 0x1c, 0x03, 0x9c, 0xff, 0x87, 0x00, 0x70, 0xe0, 0x0e, 0x1c, 0x01, 0xc3, 0x00, 0x00, 
	0x0f, 0x9e, 0x1c, 0x03, 0x9e, 0xff, 0x87, 0x00, 0x70, 0xe0, 0x1e, 0x1c, 0x03, 0xc3, 0x80, 0x60, 
	0x0f, 0xbe, 0x1e, 0x07, 0xbe, 0x3f, 0x87, 0x00, 0x70, 0x70, 0x1e, 0x0e, 0x03, 0xc3, 0x80, 0xe0, 
	0x0f, 0xbf, 0x1e, 0x07, 0x26, 0x3f, 0x07, 0x00, 0x70, 0x70, 0x1e, 0x0e, 0x07, 0xc3, 0xc0, 0xe0, 
	0x0f, 0x9f, 0x0f, 0x0f, 0x06, 0x1f, 0x07, 0x00, 0x70, 0x78, 0x3e, 0x0f, 0x07, 0xc1, 0xc1, 0xe0, 
	0x0f, 0x9f, 0x0f, 0xff, 0x06, 0x1f, 0x07, 0x00, 0x70, 0x3c, 0x6e, 0x07, 0x9d, 0xc1, 0xf7, 0xc0, 
	0x0f, 0x9f, 0x07, 0xfe, 0x06, 0x3e, 0x07, 0x00, 0x70, 0x3f, 0xee, 0x07, 0xfd, 0xc0, 0xff, 0x80, 
	0x0f, 0x9f, 0x83, 0xfc, 0xc7, 0x3e, 0x07, 0x00, 0x70, 0x1f, 0xce, 0x03, 0xf9, 0xc0, 0xff, 0x80, 
	0x0f, 0x9f, 0x83, 0xf8, 0xe7, 0xfc, 0x07, 0x00, 0x70, 0x0f, 0xce, 0x01, 0xf1, 0xc0, 0x3f, 0x00, 
	0x00, 0x00, 0x00, 0xf0, 0xef, 0xe0, 0x06, 0x00, 0x00, 0x07, 0x00, 0x00, 0x41, 0xc0, 0x1c, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xc0, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x03, 0x80, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0e, 0x03, 0x80, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0e, 0x07, 0x80, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x0f, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0xff, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0xfe, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xfc, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xfc, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// --- Status Variables ---
bool wifiConnected = false;
bool robridgeConnected = false;
bool apiProcessing = false;
String lastScannedCode = "";
unsigned long lastScanTime = 0;  // Timestamp of last scan for duplicate prevention
String lastApiResponse = "";
unsigned long lastPingTime = 0;
unsigned long pingInterval = 30000; // Ping every 30 seconds
bool isRegistered = false;
unsigned long scanCount = 0;
String deviceIP = "";  // Device IP address
String userToken = "";  // User authentication token from QR code
bool isPaired = false;  // Device paired with user account

// --- Preferences for storing server config ---
Preferences preferences;

// --- System Lock Configuration ---
const String INIT_BARCODE = "BVS-110-INI";  // Initialization barcode from quick start guide
bool systemLocked = true;  // System starts LOCKED by default - requires initialization barcode

// MAX1704X Fuel Gauge Configuration (fine-tuned calibration)
// Calibration: 1.277 × (3.89V actual / 3.99V measured) = 1.245
MAX1704X fuelGauge = MAX1704X(1.245);  // Calibrated ADC resolution
unsigned long lastBatteryUpdate = 0;
const unsigned long batteryUpdateInterval = 1000; // Update every 1 second for real-time response
float voltage = 0.0;
float bat_percentage = 0.0;

// ===== CHARGING DETECTION (Single CHRG Pin Only) =====
// CHRG pin: LOW = Charging, HIGH = Not charging (fully charged or disconnected)
#define CHARGING_PIN 27  // CHRG pin from charger module (active LOW)
bool isCharging = false;
bool isFullyCharged = false;

// ===== CHARGING ANIMATION STATE TRACKING =====
bool wasCharging = false;
unsigned long chargingStartTime = 0;
bool showFullScreenCharging = false;

// MAX1704X Battery Monitoring - Real-time with custom voltage mapping
void updateBattery() {
  // ===== SKIP ALL DISPLAY UPDATES WHEN SYSTEM IS LOCKED =====
  // When locked, only show the locked screen - no battery warnings, charging animations, etc.
  if (systemLocked) {
    // Still update battery values in background, but don't show any notifications
    unsigned long currentTime = millis();
    if (currentTime - lastBatteryUpdate < batteryUpdateInterval) {
      return;
    }
    lastBatteryUpdate = currentTime;
    
    // Read and update battery values silently
    float newVoltage = fuelGauge.voltage();
    float voltageV = newVoltage / 1000.0;
    voltage = newVoltage;
    
    // Simple percentage calculation
    if (voltageV >= 4.0) {
      bat_percentage = 100.0;
    } else if (voltageV <= 3.3) {
      bat_percentage = 0.0;
    } else {
      bat_percentage = ((voltageV - 3.3) / (4.0 - 3.3)) * 100.0;
    }
    
    if (bat_percentage > 100) bat_percentage = 100;
    if (bat_percentage < 0) bat_percentage = 0;
    
    // Update charging status silently (single CHRG pin)
    bool chrgPin = digitalRead(CHARGING_PIN);
    isCharging = !chrgPin;  // CHRG LOW = charging
    isFullyCharged = (chrgPin && bat_percentage >= 99.0);
    wasCharging = isCharging;
    
    return;  // Exit early - don't show any notifications
  }
  // ===== END SYSTEM LOCK CHECK =====
  
  // ===== CHECK CHARGING ANIMATION TIMEOUT (RUNS EVERY LOOP) =====
  // This must run every loop iteration for precise 3-second timing
  if (showFullScreenCharging && (millis() - chargingStartTime > 3000)) {
    showFullScreenCharging = false;
    Serial.println("Charging animation complete, returning to normal display");
    // Force immediate display update to show normal screen
    displayStatusScreen();
  }
  // ===== END TIMEOUT CHECK =====
  
  // Check battery every 1 second for real-time response
  unsigned long currentTime = millis();
  if (currentTime - lastBatteryUpdate < batteryUpdateInterval) {
    return; // Skip update if not enough time has passed
  }
  lastBatteryUpdate = currentTime;
  
  // Read voltage from MAX1704X fuel gauge (in mV)
  float newVoltage = fuelGauge.voltage();  // Voltage in mV
  float voltageV = newVoltage / 1000.0;    // Convert to V
  
  // ===== DETECT CHARGING STATUS (SINGLE CHRG PIN) =====
  // Read CHRG pin (active LOW)
  bool chrgPin = digitalRead(CHARGING_PIN);
  
  // Determine charging state
  // CHRG LOW = Charging in progress
  // CHRG HIGH = Not charging (could be fully charged or disconnected)
  bool newIsCharging = !chrgPin;  // CHRG=LOW means charging
  
  // Determine if fully charged based on CHRG pin HIGH + battery percentage
  // Only consider fully charged if CHRG is HIGH AND battery is ≥ 99%
  bool newIsFullyCharged = (chrgPin && bat_percentage >= 99.0);
  
  // ===== VOLTAGE SMOOTHING DURING CHARGING =====
  // During charging, voltage can spike - apply smoothing to prevent sudden jumps
  static float smoothedVoltageV = 0;
  static bool smoothingInitialized = false;
  
  // Initialize smoothed value on first run or when transitioning to charging
  if (!smoothingInitialized || (!wasCharging && newIsCharging)) {
    smoothedVoltageV = voltageV;
    smoothingInitialized = true;
  }
  
  if (newIsCharging) {
    // Apply exponential moving average for smooth charging display
    // Smoothing factor: 0.05 = extremely slow smooth changes (prevents jumps)
    float alpha = 0.05;
    smoothedVoltageV = (alpha * voltageV) + ((1.0 - alpha) * smoothedVoltageV);
    voltageV = smoothedVoltageV;  // Use smoothed voltage during charging
    
    Serial.print("⚡ CHARGING - Raw: ");
    Serial.print(newVoltage / 1000.0, 2);
    Serial.print("V, Smoothed: ");
    Serial.print(voltageV, 2);
    Serial.println("V");
  } else {
    // Not charging - use raw voltage, but update smoothed value for next time
    smoothedVoltageV = voltageV;
  }
  // ===== END VOLTAGE SMOOTHING =====
  
  // Custom percentage calculation based on voltage (3.3V-4.0V range)
  // Ignore MAX1704X internal percentage - use voltage-based calculation
  float newPercentage;
  
  if (voltageV >= 4.0) {
    newPercentage = 100.0;  // Above 4.0V = 100%
  } else if (voltageV <= 3.3) {
    newPercentage = 0.0;    // Below 3.3V = 0%
  } else {
    // Linear mapping: 3.3V-4.0V → 0%-100%
    newPercentage = ((voltageV - 3.3) / (4.0 - 3.3)) * 100.0;
  }
  
  // Check if battery changed (for instant response to drops/increases)
  bool batteryChanged = false;
  if (abs(newVoltage - voltage) > 10 || abs(newPercentage - bat_percentage) > 1) {
    batteryChanged = true;
  }
  
  // Update values
  voltage = newVoltage;
  bat_percentage = newPercentage;
  
  // Ensure percentage is within valid range
  if (bat_percentage > 100) bat_percentage = 100;
  if (bat_percentage < 0) bat_percentage = 0;
  
  // ===== DETECT CHARGING STATE TRANSITIONS =====
  // Detect charging started (transition from not charging to charging)
  if (newIsCharging && !wasCharging) {
    chargingStartTime = millis();
    showFullScreenCharging = true;
    Serial.println("Charging started! Showing full-screen animation for 3 seconds");
  }
  
  // Detect charging stopped (transition from charging to not charging) - INSTANT UPDATE
  if (!newIsCharging && wasCharging) {
    Serial.println("Charging stopped! Updating display immediately");
    isCharging = newIsCharging;
    isFullyCharged = newIsFullyCharged;
    wasCharging = newIsCharging;
    displayStatusScreen();  // Force immediate display update to remove thunder icon
    return;  // Exit early to show update immediately
  }
  
  // Detect fully charged state change - INSTANT UPDATE
  if (newIsFullyCharged != isFullyCharged) {
    Serial.println("Fully charged state changed! Updating display immediately");
    isCharging = newIsCharging;
    isFullyCharged = newIsFullyCharged;
    wasCharging = newIsCharging;
    displayStatusScreen();  // Force immediate display update
    return;  // Exit early to show update immediately
  }
  
  // Update charging states
  isCharging = newIsCharging;
  isFullyCharged = newIsFullyCharged;
  wasCharging = newIsCharging;

  // ===== LOW BATTERY WARNING SYSTEM =====
  // Show warning when battery drops to critical levels (only when NOT charging)
  static bool warning15Shown = false;
  static bool warning5Shown = false;
  static unsigned long lastWarningTime = 0;
  
  if (!isCharging && !isFullyCharged) {  // Only show warnings when not charging
    // Critical warning at 5%
    if (bat_percentage <= 5 && !warning5Shown) {
      warning5Shown = true;
      warning15Shown = true;  // Also mark 15% as shown
      lastWarningTime = millis();
      
      Serial.println("⚠️ CRITICAL: Battery at 5% - Please charge immediately!");
      
      // Show critical battery warning
      display.clearDisplay();
      display.setTextSize(2);
      display.setTextColor(SH110X_WHITE);
      display.setCursor(10, 10);
      display.println("CRITICAL");
      display.setTextSize(1);
      display.setCursor(0, 30);
      display.println("Battery: 5%");
      display.println("");
      display.println("Charge Now!");
      display.display();
      delay(3000);  // Show for 3 seconds
      
      displayStatusScreen();  // Return to normal screen
    }
    // Low battery warning at 15%
    else if (bat_percentage <= 15 && !warning15Shown) {
      warning15Shown = true;
      lastWarningTime = millis();
      
      Serial.println("⚠️ WARNING: Battery at 15% - Please charge soon");
      
      // Show low battery warning
      display.clearDisplay();
      display.setTextSize(2);
      display.setTextColor(SH110X_WHITE);
      display.setCursor(5, 15);
      display.println("LOW");
      display.println("BATTERY");
      display.setTextSize(1);
      display.setCursor(20, 50);
      display.print(bat_percentage, 0);
      display.println("%");
      display.display();
      delay(3000);  // Show for 3 seconds
      
      displayStatusScreen();  // Return to normal screen
    }
  }
  
  // Reset warning flags when battery is charged above thresholds
  if (bat_percentage > 15) {
    warning15Shown = false;
    warning5Shown = false;
  } else if (bat_percentage > 5) {
    warning5Shown = false;
  }
  // ===== END LOW BATTERY WARNING SYSTEM =====

  // ===== FULL CHARGE INDICATOR (STATUS BAR ONLY) =====
  // No full-screen popup - just show checkmark icon in status bar when fully charged
  // The displayBatteryStatus() function already handles showing checkmark when bat_percentage >= 100%
  // Just log to serial when charging completes
  static bool batteryWasFull = false;
  
  if (isFullyCharged && !isCharging && bat_percentage >= 99.0 && !batteryWasFull) {
    batteryWasFull = true;
    Serial.println("✅ Charging finished - battery fully charged to 99%+ (checkmark shown in status bar)");
  }
  
  // Reset flag when battery drops below 99% or starts charging
  if (bat_percentage < 99.0 || isCharging) {
    batteryWasFull = false;
  }
  // ===== END FULL CHARGE INDICATOR =====

  // Debug output only when battery changes (reduce serial spam)
  if (batteryChanged) {
    Serial.print("⚡ BATTERY - Voltage: ");
    Serial.print(voltageV, 2);  // Show voltage in V with 2 decimals
    Serial.print("V (");
    Serial.print(newVoltage, 0);
    Serial.print("mV) | Percentage: ");
    Serial.print(bat_percentage, 1);  // Show percentage with 1 decimal
    Serial.print("%");
    if (isCharging) Serial.print(" | CHARGING");
    if (isFullyCharged) Serial.print(" | FULL");
    Serial.println();
  }
}

// ===== BATTERY DISPLAY - AUTO-SWITCHING (DISCHARGING vs CHARGING) =====

// Draw small battery icon with fill level (DISCHARGING MODE)
void drawBatteryIcon(Adafruit_SH1106G &display, int x, int y) {
  int width = 12;   // small icon width
  int height = 6;   // small icon height
  int capWidth = 2; // battery cap width
  
  // Calculate fill width - ensure 100% shows completely full
  int maxFillWidth = width - 2;  // Maximum fill width (leave 1px border on each side)
  int fillWidth = (int)((bat_percentage / 100.0) * maxFillWidth);
  
  // Ensure at 100% it's completely full
  if (bat_percentage >= 99.5) {
    fillWidth = maxFillWidth;
  }
  
  display.drawRect(x, y, width, height, SH110X_WHITE);        // outline
  display.fillRect(x + 1, y + 1, fillWidth, height - 2, SH110X_WHITE); // fill
  display.fillRect(x + width, y + 2, capWidth, height - 4, SH110X_WHITE); // cap
}

// Draw compact charging battery icon (CHARGING MODE - ANIMATED FILLING)
// Nokia-style continuous filling animation
void drawChargingBatteryIcon(Adafruit_SH1106G &display, int x, int y) {
  int width = 12;
  int height = 6;
  int capWidth = 2;
  
  // Animated filling effect - cycles from 0% to 100% continuously
  static unsigned long lastAnimUpdate = 0;
  static int animFillLevel = 0;  // 0 to 100
  
  // Update animation every 200ms for smooth filling
  if (millis() - lastAnimUpdate > 200) {
    animFillLevel += 10;  // Increase by 10% each step
    if (animFillLevel > 100) {
      animFillLevel = 0;  // Reset to empty and start again
    }
    lastAnimUpdate = millis();
  }
  
  // Calculate fill width based on animation level
  int maxFillWidth = width - 2;
  int fillWidth = (int)((animFillLevel / 100.0) * maxFillWidth);
  
  // Draw battery outline
  display.drawRect(x, y, width, height, SH110X_WHITE);
  
  // Draw animated fill
  if (fillWidth > 0) {
    display.fillRect(x + 1, y + 1, fillWidth, height - 2, SH110X_WHITE);
  }
  
  // Draw battery cap
  display.fillRect(x + width, y + 2, capWidth, height - 4, SH110X_WHITE);
  
  // Draw mini lightning bolt (charging indicator) - 4x5 pixels
  int boltX = x + width + capWidth + 2;
  int boltY = y;
  display.fillTriangle(boltX, boltY, boltX + 2, boltY, boltX, boltY + 5, SH110X_WHITE);
  display.fillTriangle(boltX, boltY + 5, boltX + 2, boltY + 5, boltX + 4, boltY, SH110X_WHITE);
}

// Draw fully charged icon (checkmark instead of lightning)
void drawFullyChargedIcon(Adafruit_SH1106G &display, int x, int y) {
  int width = 12;
  int height = 6;
  int capWidth = 2;
  
  // Draw battery outline (fully filled)
  display.drawRect(x, y, width, height, SH110X_WHITE);
  display.fillRect(x + 1, y + 1, width - 3, height - 2, SH110X_WHITE);
  display.fillRect(x + width, y + 2, capWidth, height - 4, SH110X_WHITE);
  
  // Draw mini checkmark - 4x4 pixels
  int checkX = x + width + capWidth + 2;
  int checkY = y + 1;
  display.drawLine(checkX, checkY + 2, checkX + 1, checkY + 3, SH110X_WHITE);
  display.drawLine(checkX + 1, checkY + 3, checkX + 4, checkY, SH110X_WHITE);
  display.drawLine(checkX, checkY + 3, checkX + 1, checkY + 4, SH110X_WHITE);
  display.drawLine(checkX + 1, checkY + 4, checkX + 4, checkY + 1, SH110X_WHITE);
}

// Smart battery display - auto-switches between modes
void displayBatteryStatus(Adafruit_SH1106G &display, int x, int y) {
  // Show checkmark when battery is at 100% (charged)
  if (bat_percentage >= 100.0) {
    // Show fully charged icon (battery + checkmark)
    drawFullyChargedIcon(display, x, y);
  } else if (isCharging) {
    // Show charging icon (battery + lightning bolt)
    drawChargingBatteryIcon(display, x, y);
  } else {
    // Show normal discharging icon (battery only)
    drawBatteryIcon(display, x, y);
  }
}

// ===== FULL-SCREEN CHARGING ANIMATION FUNCTIONS =====

// Draw full-screen charging animation (shown for 3 seconds when charging starts)
void drawFullScreenCharging() {
  // Large "CHARGING" text
  display.setTextSize(2);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(10, 10);
  display.print("CHARGING");
  
  // Large battery icon
  int battX = 34;
  int battY = 35;
  int battWidth = 50;
  int battHeight = 20;
  
  display.drawRect(battX, battY, battWidth, battHeight, SH110X_WHITE);
  display.fillRect(battX + battWidth, battY + 6, 3, 8, SH110X_WHITE);
  
  // Animated fill based on time
  int frame = (millis() / 500) % 4;
  int segmentWidth = (battWidth - 4) / 4;
  for (int i = 0; i <= frame; i++) {
    display.fillRect(battX + 2 + (i * segmentWidth), battY + 2, 
                     segmentWidth - 2, battHeight - 4, SH110X_WHITE);
  }
  
  // Large lightning bolt
  drawLargeThunder(10, 38, 15);
}

// Draw mini thunder icon in top right corner (for normal display when charging)
void drawMiniThunderIcon() {
  // Mini thunder symbol in top right corner
  int x = 110;  // Position from left
  int y = 2;    // Position from top
  
  // Small lightning bolt (8 pixels)
  display.fillTriangle(x, y, x + 4, y, x, y + 8, SH110X_WHITE);
  display.fillTriangle(x, y + 8, x + 4, y + 8, x + 8, y, SH110X_WHITE);
}

// Draw fully charged icon (checkmark) in top right corner
void drawFullChargedIconCorner() {
  // Mini checkmark in top right corner
  int x = 108;
  int y = 4;
  
  display.drawLine(x, y + 4, x + 3, y + 7, SH110X_WHITE);
  display.drawLine(x + 3, y + 7, x + 8, y, SH110X_WHITE);
  display.drawLine(x, y + 5, x + 3, y + 8, SH110X_WHITE);
  display.drawLine(x + 3, y + 8, x + 8, y + 1, SH110X_WHITE);
}

// Draw large lightning bolt for full-screen animation
void drawLargeThunder(int x, int y, int size) {
  // Large lightning bolt for full screen
  display.fillTriangle(x, y, x + size/2, y, x, y + size, SH110X_WHITE);
  display.fillTriangle(x, y + size, x + size/2, y + size, x + size, y, SH110X_WHITE);
}

// ===== END FULL-SCREEN CHARGING ANIMATION FUNCTIONS =====



// --- WiFi Auto-Reconnect Variables ---
unsigned long lastWiFiCheck = 0;
unsigned long wifiCheckInterval = 5000; // Check WiFi every 5 seconds
unsigned long lastReconnectAttempt = 0;
unsigned long reconnectDelay = 1000; // Start with 1 second delay
unsigned long maxReconnectDelay = 30000; // Max 30 seconds between attempts
int reconnectAttempts = 0;
int maxReconnectAttempts = 10;
bool wifiReconnectInProgress = false;
String lastWiFiStatus = "";
int wifiRSSI = 0;
unsigned long wifiConnectedTime = 0;

// --- Debug and Utility Functions ---
void debugPrint(String message, bool newline = true) {
  String timestamp = "[" + String(millis()) + "] ";
  if (newline) {
    Serial.println(timestamp + message);
  } else {
    Serial.print(timestamp + message);
  }
}

void debugPrintWiFiStatus() {
  debugPrint("=== WiFi Status Debug ===");
  debugPrint("WiFi Status: " + String(WiFi.status()));
  debugPrint("WiFi Connected: " + String(wifiConnected ? "YES" : "NO"));
  debugPrint("SSID: " + String(WiFi.SSID()));
  debugPrint("IP Address: " + WiFi.localIP().toString());
  debugPrint("RSSI: " + String(WiFi.RSSI()) + " dBm");
  debugPrint("Reconnect Attempts: " + String(reconnectAttempts));
  debugPrint("Reconnect In Progress: " + String(wifiReconnectInProgress ? "YES" : "NO"));
  debugPrint("Uptime: " + String((millis() - wifiConnectedTime) / 1000) + " seconds");
  debugPrint("========================");
}

String getWiFiStatusString(wl_status_t status) {
  switch (status) {
    case WL_NO_SSID_AVAIL: return "NO_SSID_AVAILABLE";
    case WL_SCAN_COMPLETED: return "SCAN_COMPLETED";
    case WL_CONNECTED: return "CONNECTED";
    case WL_CONNECT_FAILED: return "CONNECT_FAILED";
    case WL_CONNECTION_LOST: return "CONNECTION_LOST";
    case WL_DISCONNECTED: return "DISCONNECTED";
    case WL_IDLE_STATUS: return "IDLE_STATUS";
    default: return "UNKNOWN(" + String(status) + ")";
  }
}

void updateWiFiStatus() {
  wl_status_t currentStatus = WiFi.status();
  String statusString = getWiFiStatusString(currentStatus);
  
  if (statusString != lastWiFiStatus) {
    debugPrint("WiFi Status Changed: " + lastWiFiStatus + " -> " + statusString);
    lastWiFiStatus = statusString;
  }
  
  if (currentStatus == WL_CONNECTED) {
    wifiRSSI = WiFi.RSSI();
    if (!wifiConnected) {
      wifiConnectedTime = millis();
      debugPrint("WiFi Connected Successfully!");
      debugPrint("IP: " + WiFi.localIP().toString());
      debugPrint("RSSI: " + String(wifiRSSI) + " dBm");
    }
    wifiConnected = true;
    reconnectAttempts = 0;
    reconnectDelay = 1000; // Reset delay
    wifiReconnectInProgress = false;
  } else {
    if (wifiConnected) {
      debugPrint("WiFi Connection Lost!");
      wifiConnected = false;
      robridgeConnected = false;
      isRegistered = false;
      
      // Show WiFi disconnected message on display
      display.clearDisplay();
      displayStatusBar();  // Show status bar with disconnected WiFi icon
      display.setCursor(0, 20);
      display.setTextSize(1);
      display.println("WiFi Disconnected");
      display.println("");
      display.println("To restart:");
      display.println("Hold trigger 10 sec");
      display.display();
    }
  }
}

bool attemptWiFiReconnect() {
  if (wifiReconnectInProgress) {
    return false;
  }
  
  unsigned long currentTime = millis();
  if (currentTime - lastReconnectAttempt < reconnectDelay) {
    return false;
  }
  
  if (reconnectAttempts >= maxReconnectAttempts) {
    debugPrint("Max reconnect attempts reached. Resetting attempts.");
    reconnectAttempts = 0;
    reconnectDelay = 1000;
  }
  
  wifiReconnectInProgress = true;
  lastReconnectAttempt = currentTime;
  reconnectAttempts++;
  
  debugPrint("WiFi Reconnect Attempt #" + String(reconnectAttempts));
  debugPrint("Current Status: " + getWiFiStatusString(WiFi.status()));
  
  // Disconnect first to ensure clean connection
  if (WiFi.status() != WL_DISCONNECTED) {
    debugPrint("Disconnecting from WiFi...");
    WiFi.disconnect(true);
    delay(1000);
  }
  
  debugPrint("Attempting to connect to: " + String(ssid));
  WiFi.begin(ssid, password);
  
  // Wait for connection with timeout
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - startTime) < 10000) {
    delay(100);
    if ((millis() - startTime) % 1000 == 0) {
      debugPrint("Connection attempt in progress...", false);
      Serial.print(".");
    }
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    debugPrint("WiFi Reconnection Successful!");
    wifiReconnectInProgress = false;
    return true;
  } else {
    debugPrint("WiFi Reconnection Failed. Status: " + getWiFiStatusString(WiFi.status()));
    wifiReconnectInProgress = false;
    
    // Exponential backoff
    reconnectDelay = min(reconnectDelay * 2, maxReconnectDelay);
    debugPrint("Next reconnect attempt in " + String(reconnectDelay / 1000) + " seconds");
    return false;
  }
}

void checkWiFiConnection() {
  unsigned long currentTime = millis();
  
  if (currentTime - lastWiFiCheck < wifiCheckInterval) {
    return;
  }
  
  lastWiFiCheck = currentTime;
  updateWiFiStatus();
  
  if (!wifiConnected && !wifiReconnectInProgress) {
    debugPrint("WiFi not connected, attempting reconnection...");
    attemptWiFiReconnect();
  }
  
  // Log periodic status
  if (wifiConnected) {
    debugPrint("WiFi Health Check - RSSI: " + String(WiFi.RSSI()) + " dBm, Uptime: " + String((millis() - wifiConnectedTime) / 1000) + "s");
  }
}

// Function to clean raw data
String cleanBarcode(String rawData) {
  Serial.println("Raw barcode data: '" + rawData + "'");
  Serial.println("Raw data length: " + String(rawData.length()));
  
  // Trim whitespace and control characters
  String cleaned = rawData;
  cleaned.trim();
  
  // Remove common control characters that might be added by the scanner
  cleaned.replace("\r", "");
  cleaned.replace("\n", "");
  cleaned.replace("\t", "");
  
  // Check if it's a URL (contains http:// or https://)
  if (cleaned.indexOf("http://") >= 0 || cleaned.indexOf("https://") >= 0) {
    Serial.println("Detected URL barcode, keeping as-is");
    Serial.println("Cleaned URL: '" + cleaned + "'");
    return cleaned;
  }
  
  // Check if it's an alphanumeric barcode (contains letters)
  bool hasLetters = false;
  for (int i = 0; i < cleaned.length(); i++) {
    char c = cleaned[i];
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
      hasLetters = true;
      break;
    }
  }
  
  if (hasLetters) {
    Serial.println("Detected alphanumeric barcode, keeping as-is");
    Serial.println("Cleaned alphanumeric: '" + cleaned + "'");
    return cleaned;
  }
  
  // For numeric-only barcodes, keep only digits
  String numericOnly = "";
  for (int i = 0; i < cleaned.length(); i++) {
    char c = cleaned[i];
    if (c >= '0' && c <= '9') {  // Keep only digits
      numericOnly += c;
    }
  }
  
  // ===== DETECT AND FIX CONCATENATED BARCODES =====
  // Check if the barcode is a repeated pattern (continuous mode issue)
  if (numericOnly.length() > 8) {  // Only check if reasonably long
    // Try different pattern lengths (8, 9, 10, 12, 13 digits - common barcode lengths)
    int patternLengths[] = {8, 9, 10, 12, 13};
    for (int i = 0; i < 5; i++) {
      int patternLen = patternLengths[i];
      if (numericOnly.length() >= patternLen * 2) {  // At least 2 repetitions
        String pattern = numericOnly.substring(0, patternLen);
        bool isRepeated = true;
        
        // Check if the pattern repeats throughout the string
        for (int pos = patternLen; pos + patternLen <= numericOnly.length(); pos += patternLen) {
          String nextChunk = numericOnly.substring(pos, pos + patternLen);
          if (nextChunk != pattern) {
            isRepeated = false;
            break;
          }
        }
        
        if (isRepeated) {
          Serial.println("⚠️ CONCATENATION DETECTED! Pattern: '" + pattern + "' repeated " + String(numericOnly.length() / patternLen) + " times");
          Serial.println("Extracting first occurrence only");
          numericOnly = pattern;  // Use only the first occurrence
          break;
        }
      }
    }
  }
  // ===== END CONCATENATION DETECTION =====
  
  Serial.println("Detected numeric barcode, cleaned: '" + numericOnly + "'");
  Serial.println("Cleaned length: " + String(numericOnly.length()));
  return numericOnly;
}


// Function to manually wake up sleeping Render servers
void wakeUpRenderServer(String serverURL) {
  debugPrint("Attempting to wake up Render server...");
  
  // Try simple HTTP GET to wake up the server
  HTTPClient http;
  String httpURL = serverURL;
  if (httpURL.startsWith("https://")) {
    httpURL = "http://" + httpURL.substring(8);
  }
  
  http.begin(httpURL + "/");
  http.setTimeout(10000);
  http.addHeader("User-Agent", "ESP32-WakeUp/1.0");
  
  int responseCode = http.GET();
  debugPrint("Wake-up response: HTTP " + String(responseCode));
  
  if (responseCode > 0) {
    String response = http.getString();
    debugPrint("Wake-up successful! Response: " + response.substring(0, min(50, (int)response.length())));
  }
  
  http.end();
  delay(2000); // Give server time to fully wake up
}

// Function to test server connectivity with retry logic for Render sleep
bool testServerConnection(String serverURL) {
  // Try multiple times to handle Render free tier sleep
  for (int attempt = 1; attempt <= 3; attempt++) {
    debugPrint("=== Connection attempt " + String(attempt) + "/3 ===");
    
    // First try HTTP (non-secure) to see if it's an SSL issue
    HTTPClient http;
    
    // Try HTTP first (remove https:// and use http://)
    String httpURL = serverURL;
    if (httpURL.startsWith("https://")) {
      httpURL = "http://" + httpURL.substring(8);
    }
    
    debugPrint("Testing HTTP connection to: " + httpURL);
    
    // Try different endpoints
    String endpoints[] = {"/api/health", "/api/esp32/scan", "/health", "/"};
    
    for (int i = 0; i < 4; i++) {
      String testURL = httpURL + endpoints[i];
      debugPrint("Testing HTTP endpoint: " + testURL);
      
      http.begin(testURL);
      http.setTimeout(15000); // Longer timeout for sleeping servers
      http.addHeader("User-Agent", "ESP32-Test/1.0");
      
      int responseCode = http.GET();
      debugPrint("HTTP Response from " + endpoints[i] + ": " + String(responseCode));
      
      if (responseCode > 0) {
        String response = http.getString();
        debugPrint("HTTP Response: " + response.substring(0, min(100, (int)response.length())));
        http.end();
        debugPrint("Server is awake and responding!");
        return true; // Found a working endpoint
      }
      
      http.end();
      delay(2000); // Wait between attempts
    }
    
    // If HTTP fails, try HTTPS with proper SSL setup
    debugPrint("HTTP failed, trying HTTPS with proper SSL setup...");
    WiFiClientSecure client;
    client.setInsecure(); // Skip certificate verification for Render.com
    
    for (int i = 0; i < 2; i++) { // Only try first 2 endpoints with HTTPS
      String testURL = serverURL + endpoints[i];
      debugPrint("Testing HTTPS endpoint: " + testURL);
      
      // Proper HTTPS setup
      if (http.begin(client, testURL)) {
        http.setTimeout(20000); // Longer timeout for HTTPS
        http.addHeader("User-Agent", "ESP32-Test/1.0");
        
        int responseCode = http.GET();
        debugPrint("HTTPS Response from " + endpoints[i] + ": " + String(responseCode));
        
        if (responseCode > 0) {
          String response = http.getString();
          debugPrint("HTTPS Response: " + response.substring(0, min(100, (int)response.length())));
          http.end();
          debugPrint("HTTPS connection successful!");
          return true;
        }
      } else {
        debugPrint("Failed to begin HTTPS connection to " + testURL);
      }
      
      http.end();
      delay(2000);
    }
    
    // If this attempt failed and we have more attempts, wait before retrying
    if (attempt < 3) {
      debugPrint("Attempt " + String(attempt) + " failed. Waiting 5 seconds before retry...");
      debugPrint("erver might be sleeping. Trying to wake it up...");
      delay(5000); // Wait 5 seconds between attempts
    }
  }
  
  debugPrint("All connection attempts failed. Server may be down or DNS issue.");
  return false;
}

// Function to analyze product using AI - Fixed Render.com connection
Product analyzeProductWithAI(String scannedCode) {
  Product product;
  product.barcode = scannedCode;
  
  if (!wifiConnected) {
    debugPrint("Cannot analyze product with AI - WiFi not connected");
    product.name = "WiFi Error";
    product.type = "Connection";
    product.details = "WiFi not connected";
    product.price = "N/A";
    product.category = "Error";
    product.location = "Unknown";
    return product;
  }
  
  debugPrint("Scanned Code: " + scannedCode);
  unsigned long analysisStartTime = millis();
  const unsigned long maxAnalysisTime = 45000; // 45 second max timeout
  
  // Try multiple connection strategies for Render.com
  HTTPClient http;
  bool connectionSuccess = false;
  String serverUrl = "";
  
  // Strategy 1: Try AI server directly (HTTP first)
  debugPrint("🔔 Strategy 1: Trying AI server directly...");
  serverUrl = aiServerURL + "/api/esp32/scan";
  http.begin(serverUrl);
  http.setTimeout(20000); // 20 second timeout for sleeping servers
  http.addHeader("Content-Type", "application/json");
  http.addHeader("User-Agent", "ESP32-Robridge/2.0");
  
  String payload = "{\"deviceId\":\"" + deviceId + "\",\"barcodeData\":\"" + scannedCode + "\",\"deviceName\":\"" + deviceName + "\",\"scanType\":\"GM77_SCAN\",\"timestamp\":" + String(millis()) + "}";
  debugPrint("Payload: " + payload);
  
  int httpResponseCode = http.POST(payload);
  debugPrint("HTTP Response Code: " + String(httpResponseCode));
  
  if (httpResponseCode == 200) {
    connectionSuccess = true;
    debugPrint("✅ HTTP connection successful!");
  } else if (httpResponseCode == 307 || httpResponseCode == 301 || httpResponseCode == 302) {
    debugPrint("🔄 HTTP redirect detected (Code: " + String(httpResponseCode) + "), following redirect...");
    http.end(); // Close HTTP connection before trying HTTPS
    connectionSuccess = false; // Ensure we don't mark as successful yet
  } else if (httpResponseCode > 0) {
    connectionSuccess = true;
    debugPrint("✅ HTTP connection successful!");
  } else {
    debugPrint("❌ HTTP failed: " + http.errorToString(httpResponseCode));
    http.end();
  }
  
  // Strategy 2: Try HTTPS if HTTP didn't work or was redirected
  if (!connectionSuccess && (httpResponseCode == 307 || httpResponseCode == 301 || httpResponseCode == 302 || httpResponseCode <= 0)) {
    debugPrint("🔔 Strategy 2: Trying HTTPS with SSL setup...");
    WiFiClientSecure secureClient;
    secureClient.setInsecure(); // Skip certificate verification for Render.com
    secureClient.setTimeout(15000); // Reduced timeout to 15 seconds
    
    serverUrl = aiServerURL + "/api/esp32/scan";
    debugPrint("Attempting HTTPS connection to: " + serverUrl);
    
    if (http.begin(secureClient, serverUrl)) {
      debugPrint("✅ HTTPS connection initiated");
      http.setTimeout(15000); // 15 second timeout
      http.addHeader("Content-Type", "application/json");
      http.addHeader("User-Agent", "ESP32-Robridge/2.0");
      
      debugPrint("HTTPS Payload: " + payload);
      
      // Check timeout before making request
      if (millis() - analysisStartTime > maxAnalysisTime) {
        debugPrint("⏰ Analysis timeout reached, aborting HTTPS attempt");
        http.end();
        httpResponseCode = -1;
        connectionSuccess = false;
      } else {
        debugPrint("Sending HTTPS POST request...");
        httpResponseCode = http.POST(payload);
        debugPrint("HTTPS Response Code: " + String(httpResponseCode));
      }
    } else {
      debugPrint("❌ Failed to begin HTTPS connection");
      httpResponseCode = -1;
    }
    
    if (httpResponseCode > 0) {
      connectionSuccess = true;
      debugPrint("✅ HTTPS connection successful!");
    } else {
      debugPrint("❌ HTTPS failed: " + http.errorToString(httpResponseCode));
      http.end();
      
      // Strategy 3: Try alternative AI server
      debugPrint("🔔 Strategy 3: Trying alternative AI server...");
      serverUrl = aiServerURL + "/api/esp32/scan";
      http.begin(secureClient, serverUrl);
      http.setTimeout(30000);
      http.addHeader("Content-Type", "application/json");
      http.addHeader("User-Agent", "ESP32-Robridge/2.0");
      
      debugPrint("Alternative Payload: " + payload);
      httpResponseCode = http.POST(payload);
      debugPrint("Alternative Response Code: " + String(httpResponseCode));
      
      if (httpResponseCode > 0) {
        connectionSuccess = true;
        debugPrint("✅ Alternative server connection successful!");
      } else {
        debugPrint("❌ All connection strategies failed");
        http.end();
      }
    }
  }
  
  if (connectionSuccess && httpResponseCode == 200) {
    String response = http.getString();
    debugPrint("Response: " + response);
    
    // Parse JSON response
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, response);
    
    if (!error) {
      // Parse AI server response format (AIAnalysisResponse)
      if (doc["title"]) {
        String title = doc["title"] | "Unknown Product";
        String category = doc["category"] | "Unknown";
        String description = doc["description"] | "No description available";
        
        debugPrint("✅ AI Analysis Success!");
        debugPrint("Title: " + title);
        debugPrint("Category: " + category);
        
        // Fill product info for display
        product.name = title;
        product.type = category;
        product.details = description;
        product.price = "N/A";
        product.category = category;
        product.location = "Unknown";
      } else {
        debugPrint("❌ No title in response");
        product.name = "Scanned Code: " + scannedCode;
        product.type = "Parse Error";
        product.details = "No title in AI server response";
        product.price = "N/A";
        product.category = "Unknown";
        product.location = "Unknown";
      }
      
    } else {
      debugPrint("❌ JSON parse failed: " + String(error.c_str()));
      product.name = "Scanned Code: " + scannedCode;
      product.type = "Parse Error";
      product.details = "JSON parsing failed: " + String(error.c_str());
      product.price = "N/A";
      product.category = "Unknown";
      product.location = "Unknown";
    }
  } else if (connectionSuccess) {
    // Server responded but with error code
    String response = http.getString();
    debugPrint("Server Error Response: " + response);
    debugPrint("Response length: " + String(response.length()));
    
    if (response.length() == 0) {
      debugPrint("⚠️ Empty response - server might be redirecting or have an issue");
      product.name = "Scanned Code: " + scannedCode;
      product.type = "Redirect/Empty";
      product.details = "Server returned empty response (HTTP " + String(httpResponseCode) + ")";
      product.price = "N/A";
      product.category = "Unknown";
      product.location = "Unknown";
    } else {
      product.name = "Scanned Code: " + scannedCode;
      product.type = "Server Error";
      product.details = "HTTP " + String(httpResponseCode) + ": " + response;
      product.price = "N/A";
      product.category = "Unknown";
      product.location = "Unknown";
    }
  } else {
    // All connection attempts failed
    product.name = "Scanned Code: " + scannedCode;
    product.type = "Connection Failed";
    product.details = "Cannot connect to AI servers. Check internet connection.";
    product.price = "N/A";
    product.category = "Unknown";
    product.location = "Unknown";
  }
  
  http.end();
  
  // Final timeout check
  unsigned long analysisTime = millis() - analysisStartTime;
  debugPrint("Analysis completed in " + String(analysisTime) + "ms");
  
  if (analysisTime > maxAnalysisTime) {
    debugPrint("⚠️ Analysis took too long, may have timed out");
    product.name = "Scanned Code: " + scannedCode;
    product.type = "Timeout";
    product.details = "Analysis timed out after " + String(analysisTime) + "ms";
    product.price = "N/A";
    product.category = "Unknown";
    product.location = "Unknown";
  }
  
  return product;
}

// ---------------------------------------------------------------
// Helper function to check for 10-second trigger hold restart
// Can be called from anywhere, including blocking operations
// ---------------------------------------------------------------
void checkTriggerRestart() {
  static unsigned long triggerPressStart = 0;
  static bool triggerHoldDetected = false;
  
  int buttonState = digitalRead(GM77_TRIG_PIN);
  
  if (buttonState == LOW) {  // Trigger button pressed
    if (triggerPressStart == 0) {
      triggerPressStart = millis();
      triggerHoldDetected = false;
      Serial.println("🔘 Trigger button PRESSED - hold for 10 seconds to restart");
    }
    
    unsigned long holdDuration = millis() - triggerPressStart;
    
    // Check if held for 10 seconds
    if (holdDuration >= 10000 && !triggerHoldDetected) {
      triggerHoldDetected = true;
      Serial.println("✅ 10 SECONDS REACHED - RESTARTING NOW!");
      
      // Show restart message
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SH110X_WHITE);
      display.setCursor(0, 20);
      display.println("Scanner Restarting...");
      display.println("");
      display.println("Please wait...");
      display.display();
      delay(1000);
      
      ESP.restart();
    }
  } else {
    // Button released - reset timer
    if (triggerPressStart != 0) {
      Serial.println("🔘 Trigger button RELEASED");
    }
    triggerPressStart = 0;
    triggerHoldDetected = false;
  }
}


/* ----------------------------------------------------------
   Auto-connect + OLED feedback  (requirement 1 & 2)
---------------------------------------------------------- */
void connectWiFi(){
  display.clearDisplay();
  displayStatusBar();
  display.setCursor(0,20);
  display.println(F("Auto-connecting..."));
  display.display();

  // ADD THIS: Clean disconnect first (helps after restart)10
  WiFi.disconnect(false);  // false = keep credentials
  delay(1000);  // Give it time to disconnect
  
  WiFi.mode(WIFI_STA);
  WiFi.begin();                             
  uint8_t tries = 0;                        
  while (WiFi.status() != WL_CONNECTED && tries < 13){  // 13 seconds
    // Check for trigger restart during WiFi connection
    checkTriggerRestart();
    delay(100);  // Small delay, check trigger 10 times per second
    
    // Count as 1 second after 10 iterations
    static int delayCount = 0;
    delayCount++;
    if (delayCount >= 10) {
      tries++;
      delayCount = 0;
    }
  }

  if (WiFi.status() == WL_CONNECTED){       
    deviceIP = WiFi.localIP().toString();
    wifiConnected = true;
    Serial.println("\nWiFi connected (auto)");
    Serial.println("IP: " + deviceIP);
    
    // Show success message
    display.clearDisplay();
    displayStatusBar();
    display.setCursor(0, 20);
    display.println(F("WiFi Connected!"));
    display.setCursor(0, 35);
    display.println(F("IP: "));
    display.println(deviceIP);
    display.display();
    delay(2000);  // Show for 2 seconds
    
    loadServerConfig();
    registerWithRobridge();
    return;
  }

  // ===== CHECK WHY CONNECTION FAILED =====
  wl_status_t wifiStatus = WiFi.status();
  
  if (wifiStatus == WL_CONNECT_FAILED || wifiStatus == WL_NO_SSID_AVAIL) {
    // Connection failed - likely incorrect password or SSID not found
    debugPrint("WiFi connection failed - Status: " + getWiFiStatusString(wifiStatus));
    
    // Display error message on OLED
    display.clearDisplay();
    displayStatusBar();
    display.setCursor(0, 15);
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    
    if (wifiStatus == WL_CONNECT_FAILED) {
      display.println(F("WiFi Error:"));
      display.println(F(""));
      display.println(F("Incorrect Password"));
      display.println(F("or Auth Failed"));
    } else if (wifiStatus == WL_NO_SSID_AVAIL) {
      display.println(F("WiFi Error:"));
      display.println(F(""));
      display.println(F("Network Not Found"));
    }
    
    display.println(F(""));
    display.println(F("Starting config..."));
    display.display();
    delay(3000);  // Show error for 3 seconds
  }
  // ===== END CONNECTION FAILURE CHECK =====


  /* --------------------------------------------------------
     Auto-connect failed  -> show manual message + portal
     -------------------------------------------------------- */
  display.clearDisplay();
  displayStatusBar();
  display.setCursor(0,20);
  display.println(F("Manual connect"));
  display.setCursor(0,30);
  display.println(F("AP: bvs-scanner-id"));
  display.setCursor(0,40);
  display.println(F("PWD: rob123456"));
  display.display();

  // CRITICAL FIX: Reset saved credentials if auto-connect failed
  // This ensures a fresh start and prevents stuck AP mode
  debugPrint("Auto-connect failed - resetting WiFiManager credentials");
  
  // ===== CREDENTIAL VALIDATION WITH CALLBACK =====
  bool credentialsValid = false;
  int maxAttempts = 3;
  
  for (int attempt = 1; attempt <= maxAttempts && !credentialsValid; attempt++) {
    debugPrint("WiFiManager attempt #" + String(attempt) + "/" + String(maxAttempts));
    
    WiFiManager wm;
    wm.resetSettings();  // Reset before each attempt
    delay(500);
    
    // Add custom parameter for server IP
    WiFiManagerParameter custom_server_ip("server_ip", "Server IP Address", customServerIP.c_str(), 40);
    wm.addParameter(&custom_server_ip);
    
    // Set config portal timeout to 4 minutes (240 seconds)
    wm.setConfigPortalTimeout(240);  // 4 minutes
    
    // Disable auto-connect to manually verify credentials
    wm.setConnectRetries(3);  // Try 3 times to connect
    wm.setConnectTimeout(10);  // 10 seconds per attempt
    
    debugPrint("Starting WiFiManager portal...");
    
    // Start portal and wait for user to submit credentials
    bool portalResult = wm.autoConnect("bvs-scanner-id", "rob123456");
    
    if (!portalResult) {
      // Portal timeout or user didn't submit - enter sleep mode instead of restart
      debugPrint("WiFiManager portal timeout - entering sleep mode...");
      display.clearDisplay();
      displayStatusBar();
      display.setCursor(0, 20);
      display.println(F("Portal Timeout"));
      display.println(F("Entering Sleep..."));
      display.display();
      delay(2000);
      
      // Enter light sleep mode
      enterLightSleep();
      
      // After waking up, return to show the portal again
      continue;  // Retry the portal
    }
    
    // Portal closed - credentials were submitted
    debugPrint("Credentials submitted, verifying connection...");
    
    // Show connecting message
    display.clearDisplay();
    displayStatusBar();
    display.setCursor(0, 18);
    display.println(F("Connecting to:"));
    display.setCursor(0, 28);
    display.println(WiFi.SSID());
    display.setCursor(0, 40);
    display.println(F("Verifying..."));
    display.display();
    
    // Give it a moment to stabilize (with trigger check)
    for (int i = 0; i < 20; i++) {
      checkTriggerRestart();
      delay(100);  // 20 x 100ms = 2 seconds total
    }
    
    // Check connection status
    wl_status_t status = WiFi.status();
    debugPrint("Connection status: " + getWiFiStatusString(status));
    
    if (status == WL_CONNECTED) {
      // Success!
      credentialsValid = true;
      debugPrint("✅ WiFi connected successfully!");
      
      // Save custom server IP if provided
      String newServerIP = custom_server_ip.getValue();
      if (newServerIP.length() > 0) {
        customServerIP = newServerIP;
        saveServerConfig();
        updateServerURLs();
        Serial.println("Custom server IP saved: " + customServerIP);
      }
      
      deviceIP = WiFi.localIP().toString();
      wifiConnected = true;
      Serial.println("\nWiFi connected (portal)");
      Serial.println("IP: " + deviceIP);
      
    } else {
      // Connection failed - show error
      debugPrint("❌ Connection failed with status: " + getWiFiStatusString(status));
      
      // Display specific error on OLED
      display.clearDisplay();
      displayStatusBar();
      display.setCursor(0, 12);
      display.setTextSize(1);
      display.setTextColor(SH110X_WHITE);
      
      display.println(F("WiFi Error!"));
      display.println(F(""));
      
      // Show specific error message
      if (status == WL_CONNECT_FAILED) {
        display.println(F("Wrong Password!"));
        display.println(F(""));
        display.println(F("Check credentials"));
      } else if (status == WL_NO_SSID_AVAIL) {
        display.println(F("Network Not Found"));
        display.println(F(""));
        display.println(F("Check SSID name"));
      } else if (status == WL_DISCONNECTED) {
        display.println(F("Auth Failed"));
        display.println(F(""));
        display.println(F("Check password"));
      } else {
        display.println(F("Connection Failed"));
        display.print(F("Status: "));
        display.println(status);
      }
      
      if (attempt < maxAttempts) {
        display.println(F(""));
        display.println(F("Retry in 4 sec..."));
        display.display();
        delay(4000);
        
        // Show retry message
        display.clearDisplay();
        displayStatusBar();
        display.setCursor(0, 20);
        display.println(F("Retrying..."));
        display.setCursor(0, 32);
        display.print(F("Attempt "));
        display.print(attempt + 1);
        display.print(F("/"));
        display.println(maxAttempts);
        display.display();
        delay(1000);
      } else {
        // Max attempts reached
        display.println(F(""));
        display.println(F("Max attempts!"));
        display.display();
        delay(3000);
      }
      
      // Disconnect for retry
      WiFi.disconnect(true);
      delay(1000);
    }
  }
  
  // If still not connected after all attempts, restart
  if (!credentialsValid) {
    debugPrint("Failed to connect after " + String(maxAttempts) + " attempts - restarting...");
    display.clearDisplay();
    displayStatusBar();
    display.setCursor(0, 20);
    display.println(F("Connection Failed"));
    display.println(F(""));
    display.println(F("Restarting..."));
    display.display();
    delay(3000);
    ESP.restart();
  }
  // ===== END CREDENTIAL VALIDATION =====

  loadServerConfig();                       // Load saved server config
  registerWithRobridge();
}

// Function to save server configuration to preferences
void saveServerConfig() {
  preferences.begin("robridge", false);
  preferences.putString("server_ip", customServerIP);
  preferences.end();
  debugPrint("Server config saved: " + customServerIP);
}

// Function to load server configuration from preferences
void loadServerConfig() {
  preferences.begin("robridge", true);
  customServerIP = preferences.getString("server_ip", "");
  preferences.end();
  
  if (customServerIP.length() > 0) {
    updateServerURLs();
    debugPrint("Server config loaded: " + customServerIP);
  } else {
    debugPrint("No custom server config found, using default cloud URLs");
  }
}

// Function to update server URLs based on custom IP
void updateServerURLs() {
  if (customServerIP.length() > 0) {
    // Use custom IP for local server
    expressServerURL = "http://" + customServerIP + ":3000";
    aiServerURL = "http://" + customServerIP + ":10000";
    debugPrint("Updated server URLs to use custom IP:");
    debugPrint("Express: " + expressServerURL);
    debugPrint("AI: " + aiServerURL);
  } else {
    // Use default cloud URLs
    expressServerURL = "https://robridge-express-zl9j.onrender.com";
    aiServerURL = "https://robridge-ai-tgc9.onrender.com";  // AI server - Render deployed
    debugPrint("Using default cloud server URLs");
  }
}

// ---------------------------------------------------------------
// System Lock State Management Functions
// ---------------------------------------------------------------

// Function to load system lock state from preferences
void loadLockState() {
  preferences.begin("robridge", true);
  systemLocked = preferences.getBool("sys_locked", true);  // Default to locked
  preferences.end();
  
  if (systemLocked) {
    debugPrint("System is LOCKED - initialization barcode required");
  } else {
    debugPrint("System is UNLOCKED - ready for normal operation");
  }
}

// Function to save unlock state to preferences (called once when unlocked)
void saveLockState() {
  preferences.begin("robridge", false);
  preferences.putBool("sys_locked", false);  // Save unlocked state
  preferences.end();
  debugPrint("System unlock state saved to non-volatile memory");
}

// Function to reset lock state to locked (for testing or factory reset)
void resetLockState() {
  preferences.begin("robridge", false);
  preferences.putBool("sys_locked", true);  // Force locked state
  preferences.end();
  systemLocked = true;
  debugPrint("System lock state RESET to locked - initialization barcode required");
}

// Function to display locked screen (blank or minimal message)
void displayLockedScreen() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 20);
  display.println("  System Locked");
  display.println("");
  display.println("  Scan Init Code");
  display.println("  to Activate");
  display.display();
}

// Function to unlock system and perform full initialization
void unlockSystem() {
  debugPrint("=== UNLOCKING SYSTEM ===");
  
  // Show unlock message
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 10);
  display.println("  Initialization");
  display.println("  Code Accepted!");
  display.println("");
  display.println("  Activating...");
  display.display();
  delay(2000);
  
  // Save unlock state permanently
  saveLockState();
  systemLocked = false;
  
  // Show logo
  debugPrint("Displaying startup logo...");
  display.clearDisplay();
  display.drawBitmap(0, 0, epd_bitmap_ro_bridge, 128, 64, 1);
  display.display();
  delay(3000);
  
  // Initialize WiFi and register with server
  debugPrint("Starting WiFi connection process...");
  connectWiFi();
  
  // Show ready message
  debugPrint("System initialization complete. Showing status screen...");
  delay(500);  // Small delay to ensure previous display update completes
  displayStatusScreen();
  
  debugPrint("=== SYSTEM UNLOCKED AND READY ===");
}



void displayStatusBar() {
  // Move status bar down to avoid overlap with main content
  display.drawLine(0, 10, 127, 10, SH110X_WHITE);
  // WiFi
  if (WiFi.status() == WL_CONNECTED) {
    display.fillRect(2, 7, 2, 2, SH110X_WHITE);
    display.fillRect(5, 5, 2, 4, SH110X_WHITE);
    display.fillRect(8, 3, 2, 6, SH110X_WHITE);
    display.fillRect(11, 2, 2, 7, SH110X_WHITE);
  } else {
    display.drawLine(2, 2, 12, 9, SH110X_WHITE);
    display.drawLine(12, 2, 2, 9, SH110X_WHITE);
  }
  // Battery with charging indicator - shows lightning/checkmark when charging
  updateBattery();                    
  displayBatteryStatus(display, 100, 2);  // Use smart battery display with charging icons
  // Device ID - smaller font to avoid overlap
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(30, 2);
  display.print("RobridgeAI");
}

// Function to register with Robridge server - Enhanced connection
void registerWithRobridge() {
  if (!wifiConnected) {
    debugPrint("Cannot register with Robridge - WiFi not connected");
    return;
  }
  
  debugPrint("=== Registering with Robridge Server ===");
  
  // Create JSON payload
  StaticJsonDocument<200> doc;
  doc["deviceId"] = deviceId;
  doc["deviceName"] = deviceName;
  doc["ipAddress"] = WiFi.localIP().toString();
  doc["firmwareVersion"] = firmwareVersion;
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  debugPrint("Registration Payload: " + jsonString);
  
  HTTPClient http;
  bool registrationSuccess = false;
  
  // Try HTTP first
  String registerUrl = expressServerURL + "/api/esp32/register";
  debugPrint("Trying HTTP registration: " + registerUrl);
  
  http.begin(registerUrl);
  http.setTimeout(20000);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("User-Agent", "ESP32-Robridge/2.0");
  
  int httpResponseCode = http.POST(jsonString);
  debugPrint("HTTP Registration Response: " + String(httpResponseCode));
  
  if (httpResponseCode == 200) {
    registrationSuccess = true;
    debugPrint("✅ HTTP registration successful!");
  } else if (httpResponseCode == 307 || httpResponseCode == 301 || httpResponseCode == 302) {
    debugPrint("🔄 HTTP redirect detected (Code: " + String(httpResponseCode) + "), following redirect...");
    http.end();
    registrationSuccess = false;
  } else if (httpResponseCode > 0) {
    registrationSuccess = true;
    debugPrint("✅ HTTP registration successful!");
  } else {
    debugPrint("❌ HTTP registration failed: " + http.errorToString(httpResponseCode));
    http.end();
  }
  
  // Try HTTPS if HTTP didn't work or was redirected
  if (!registrationSuccess && (httpResponseCode == 307 || httpResponseCode == 301 || httpResponseCode == 302 || httpResponseCode <= 0)) {
    debugPrint("Trying HTTPS registration...");
    WiFiClientSecure secureClient;
    secureClient.setInsecure();
    secureClient.setTimeout(30000);
    
    registerUrl = expressServerURL + "/api/esp32/register";
    http.begin(secureClient, registerUrl);
    http.setTimeout(30000);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("User-Agent", "ESP32-Robridge/2.0");
    
    httpResponseCode = http.POST(jsonString);
    debugPrint("HTTPS Registration Response: " + String(httpResponseCode));
    
    if (httpResponseCode > 0) {
      registrationSuccess = true;
      debugPrint("✅ HTTPS registration successful!");
    } else {
      debugPrint("❌ HTTPS registration failed: " + http.errorToString(httpResponseCode));
    }
  }
  
  if (registrationSuccess) {
    String response = http.getString();
    debugPrint("Registration Response: " + response);
    
    if (httpResponseCode == 200) {
      isRegistered = true;
      robridgeConnected = true;
      debugPrint("✅ Registered with Robridge successfully!");
    } else {
      debugPrint("⚠️ Registration response: HTTP " + String(httpResponseCode));
      robridgeConnected = false;
    }
  } else {
    debugPrint("❌ Robridge registration failed - all connection attempts failed");
    robridgeConnected = false;
  }
  
  http.end();
  debugPrint("=== Registration Complete ===");
}

// ======================
// DEVICE PAIRING FUNCTIONS
// ======================

// Parse pairing QR code and extract JWT token
// QR Format from mobile app: Contains JWT token for authentication
bool parsePairingQR(String qrData) {
  debugPrint("Parsing pairing QR code...");
  debugPrint("QR Data length: " + String(qrData.length()));
  
  // The QR code should contain the JWT token directly
  // Mobile app generates QR with format: "ROBRIDGE_PAIR|<jwt_token>|<user_id>"
  if (qrData.startsWith("ROBRIDGE_PAIR|")) {
    int firstPipe = qrData.indexOf('|');
    int secondPipe = qrData.indexOf('|', firstPipe + 1);
    
    if (firstPipe > 0 && secondPipe > 0) {
      userToken = qrData.substring(firstPipe + 1, secondPipe);
      debugPrint("✅ Extracted JWT token (first 20 chars): " + userToken.substring(0, min(20, (int)userToken.length())) + "...");
      return true;
    } else {
      debugPrint("❌ Invalid QR format - missing pipe separators");
      return false;
    }
  } else {
    debugPrint("❌ Not a pairing QR code - doesn't start with ROBRIDGE_PAIR|");
    return false;
  }
}

// Pair device with user account using JWT token
void pairDeviceWithUser() {
  if (!wifiConnected) {
    debugPrint("❌ Cannot pair - WiFi not connected");
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    display.setCursor(0, 0);
    display.println("Pairing Failed");
    display.println("");
    display.println("WiFi not connected");
    display.display();
    delay(3000);
    return;
  }
  
  if (userToken.length() == 0) {
    debugPrint("❌ Cannot pair - No user token");
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    display.setCursor(0, 0);
    display.println("Pairing Failed");
    display.println("");
    display.println("No token found");
    display.display();
    delay(3000);
    return;
  }
  
  debugPrint("=== Pairing Device with User ===");
  
  // Show pairing in progress
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  display.println("Pairing Device...");
  display.println("");
  display.println("Connecting to");
  display.println("server...");
  display.display();
  
  // Create JSON payload
  StaticJsonDocument<200> doc;
  doc["deviceId"] = deviceId;
  doc["deviceName"] = deviceName;
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  debugPrint("Pairing Payload: " + jsonString);
  
  HTTPClient http;
  String pairUrl = expressServerURL + "/api/devices/pair";
  debugPrint("Pairing URL: " + pairUrl);
  
  http.begin(pairUrl);
  http.setTimeout(20000);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + userToken);
  http.addHeader("User-Agent", "ESP32-Robridge/2.0");
  
  int httpResponseCode = http.POST(jsonString);
  debugPrint("Pairing Response Code: " + String(httpResponseCode));
  
  if (httpResponseCode == 200) {
    String response = http.getString();
    debugPrint("Pairing Response: " + response);
    
    isPaired = true;
    debugPrint("✅ Device paired successfully!");
    
    // Show success message
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    display.setCursor(0, 0);
    display.println("Paired!");
    display.println("");
    display.setTextSize(2);
    display.println(deviceName);
    display.setTextSize(1);
    display.println("");
    display.println("Ready to scan");
    display.display();
    delay(3000);
    
    // Show ready screen
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    display.setCursor(0, 0);
    display.println("RoBridge Scanner");
    display.println("Status: Paired");
    display.println("");
    display.println("Scan a barcode");
    display.println("to get started");
    display.display();
    
  } else {
    String response = http.getString();
    debugPrint("❌ Pairing failed: " + String(httpResponseCode));
    debugPrint("Error response: " + response);
    
    // Show error message
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    display.setCursor(0, 0);
    display.println("Pairing Failed");
    display.println("");
    display.println("Error: " + String(httpResponseCode));
    display.println("");
    display.println("Please try again");
    display.display();
    delay(3000);
  }
  
  http.end();
  debugPrint("=== Pairing Complete ===");
}

// Function to send ping to Robridge server - Enhanced connection
void sendPingToRobridge() {
  if (!isRegistered || !wifiConnected) {
    debugPrint("Cannot ping Robridge - not registered or WiFi disconnected");
    return;
  }
  
  debugPrint("Sending ping to Robridge server...");
  
  HTTPClient http;
  bool pingSuccess = false;
  
  // Try HTTP first
  String pingUrl = expressServerURL + "/api/esp32/ping/" + deviceId;
  debugPrint("Trying HTTP ping: " + pingUrl);
  
  http.begin(pingUrl);
  http.setTimeout(15000);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("User-Agent", "ESP32-Robridge/2.0");
  
  int httpResponseCode = http.POST("{}");
  debugPrint("HTTP Ping Response: " + String(httpResponseCode));
  
  if (httpResponseCode == 200) {
    pingSuccess = true;
    debugPrint("✅ HTTP ping successful!");
  } else if (httpResponseCode == 307 || httpResponseCode == 301 || httpResponseCode == 302) {
    debugPrint("🔄 HTTP redirect detected (Code: " + String(httpResponseCode) + "), following redirect...");
    http.end();
    pingSuccess = false;
  } else if (httpResponseCode > 0) {
    pingSuccess = true;
    debugPrint("✅ HTTP ping successful!");
  } else {
    debugPrint("❌ HTTP ping failed: " + http.errorToString(httpResponseCode));
    http.end();
  }
  
  // Try HTTPS if HTTP didn't work or was redirected
  if (!pingSuccess && (httpResponseCode == 307 || httpResponseCode == 301 || httpResponseCode == 302 || httpResponseCode <= 0)) {
    
    // Try HTTPS
    debugPrint("Trying HTTPS ping...");
    WiFiClientSecure secureClient;
    secureClient.setInsecure();
    secureClient.setTimeout(20000);
    
    pingUrl = expressServerURL + "/api/esp32/ping/" + deviceId;
    http.begin(secureClient, pingUrl);
    http.setTimeout(20000);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("User-Agent", "ESP32-Robridge/2.0");
    
    httpResponseCode = http.POST("{}");
    debugPrint("HTTPS Ping Response: " + String(httpResponseCode));
    
    if (httpResponseCode > 0) {
      pingSuccess = true;
      debugPrint("✅ HTTPS ping successful!");
    } else {
      debugPrint("❌ HTTPS ping failed: " + http.errorToString(httpResponseCode));
    }
  }
  
  if (pingSuccess) {
    if (httpResponseCode == 200) {
      debugPrint("✅ Ping to Robridge successful");
      robridgeConnected = true;
    } else if (httpResponseCode == 404) {
      debugPrint("⚠️ Device not found (404), attempting re-registration...");
      isRegistered = false;
      robridgeConnected = false;
      registerWithRobridge();
    } else {
      debugPrint("⚠️ Ping response: HTTP " + String(httpResponseCode));
    }
  } else {
    debugPrint("❌ All ping attempts failed");
  }
  
  http.end();
}

// Function to send barcode scan to Robridge server - Enhanced connection
void sendScanToRobridge(String barcodeData, Product* product = nullptr) {
  if (!wifiConnected) {
    debugPrint("Cannot send scan to Robridge - WiFi disconnected");
    return;
  }
  
  debugPrint("=== Sending Scan to Robridge ===");
  
  // Create JSON payload
  StaticJsonDocument<500> doc;
  doc["barcodeData"] = barcodeData;
  doc["scanType"] = "GM77_SCAN";
  doc["timestamp"] = getCurrentTimestamp();
  
  // Include product information if found
  if (product != nullptr) {
    doc["productName"] = product->name;
    doc["productType"] = product->type;
    doc["productDetails"] = product->details;
    doc["productPrice"] = product->price;
    doc["productCategory"] = product->category;
    doc["productLocation"] = product->location;
    doc["source"] = "ai_analysis";
    debugPrint("Product found: " + product->name + " (" + product->type + ")");
    debugPrint("Source: AI Analysis");
  } else {
    doc["source"] = "unknown";
    debugPrint("Product not found - no data available");
  }
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  debugPrint("Scan Payload: " + jsonString);
  
  HTTPClient http;
  bool scanSuccess = false;
  
  // Try HTTP first
  String scanUrl = expressServerURL + "/api/esp32/scan/" + deviceId;
  debugPrint("Trying HTTP scan: " + scanUrl);
  
  http.begin(scanUrl);
  http.setTimeout(20000);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("User-Agent", "ESP32-Robridge/2.0");
  
  // Add user token if device is paired for data isolation
  if (isPaired && userToken.length() > 0) {
    http.addHeader("Authorization", "Bearer " + userToken);
    debugPrint("✅ Adding user token for authenticated scan");
  } else {
    debugPrint("⚠️ No user token - scan will not be user-specific");
  }
  
  int httpResponseCode = http.POST(jsonString);
  debugPrint("HTTP Scan Response: " + String(httpResponseCode));
  
  if (httpResponseCode == 200) {
    scanSuccess = true;
    debugPrint("✅ HTTP scan successful!");
  } else if (httpResponseCode == 307 || httpResponseCode == 301 || httpResponseCode == 302) {
    debugPrint("🔄 HTTP redirect detected (Code: " + String(httpResponseCode) + "), following redirect...");
    http.end();
    scanSuccess = false;
  } else if (httpResponseCode > 0) {
    scanSuccess = true;
    debugPrint("✅ HTTP scan successful!");
  } else {
    debugPrint("❌ HTTP scan failed: " + http.errorToString(httpResponseCode));
    http.end();
  }
  
  // Try HTTPS if HTTP didn't work or was redirected
  if (!scanSuccess && (httpResponseCode == 307 || httpResponseCode == 301 || httpResponseCode == 302 || httpResponseCode <= 0)) {
    
    // Try HTTPS
    debugPrint("Trying HTTPS scan...");
    WiFiClientSecure secureClient;
    secureClient.setInsecure();
    secureClient.setTimeout(30000);
    
    scanUrl = expressServerURL + "/api/esp32/scan/" + deviceId;
    http.begin(secureClient, scanUrl);
    http.setTimeout(30000);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("User-Agent", "ESP32-Robridge/2.0");
    
    // Add user token if device is paired
    if (isPaired && userToken.length() > 0) {
      http.addHeader("Authorization", "Bearer " + userToken);
    }
    
    httpResponseCode = http.POST(jsonString);
    debugPrint("HTTPS Scan Response: " + String(httpResponseCode));
    
    if (httpResponseCode > 0) {
      scanSuccess = true;
      debugPrint("✅ HTTPS scan successful!");
    } else {
      debugPrint("❌ HTTPS scan failed: " + http.errorToString(httpResponseCode));
    }
  }
  
  if (scanSuccess) {
    String response = http.getString();
    debugPrint("Robridge scan response: " + response);
    
    if (httpResponseCode == 200) {
      debugPrint("✅ Scan sent to Robridge successfully!");
      scanCount++;
      
      // Parse response to get scan ID
      StaticJsonDocument<100> responseDoc;
      deserializeJson(responseDoc, response);
      
      if (responseDoc["success"]) {
        String scanId = responseDoc["scanId"];
        debugPrint("Robridge Scan ID: " + scanId);
      }
    } else {
      debugPrint("⚠️ Scan response: HTTP " + String(httpResponseCode));
    }
  } else {
    debugPrint("❌ Failed to send scan to Robridge - all connection attempts failed");
  }
  
  http.end();
  debugPrint("=== Scan Send Complete ===");
}

// Function to call Gemini API
String callGeminiAPI(String barcodeData) {
  if (!wifiConnected) {
    return "WiFi not connected";
  }
  
  HTTPClient http;
  http.begin(gemini_api_url + String("?key=") + gemini_api_key);
  http.addHeader("Content-Type", "application/json");
  
  // Create JSON payload
  String jsonPayload = "{";
  jsonPayload += "\"contents\":[{";
  jsonPayload += "\"parts\":[{";
  jsonPayload += "\"text\":\"Analyze this barcode data and provide information about the product: " + barcodeData + "\"";
  jsonPayload += "}]";
  jsonPayload += "}]";
  jsonPayload += "}";
  
  int httpResponseCode = http.POST(jsonPayload);
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    http.end();
    
    // Parse JSON response
    DynamicJsonDocument doc(2048);
    deserializeJson(doc, response);
    
    if (doc["candidates"][0]["content"]["parts"][0]["text"]) {
      return doc["candidates"][0]["content"]["parts"][0]["text"].as<String>();
    } else {
      return "Error parsing API response";
    }
  } else {
    http.end();
    return "API Error: " + String(httpResponseCode);
  }
}

// Function to display text with scrolling capability
void displayText(String text, int startY = 0) {
  display.clearDisplay();
  displayStatusBar(); // Always show status bar
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  
  // Start content below status bar (y=12)
  int contentStartY = max(startY, 12);
  int y = contentStartY;
  int maxCharsPerLine = 20; // Reduced to prevent overlapping
  int maxLines = (SCREEN_HEIGHT - contentStartY) / 8;
  int currentLine = 0;
  
  // Split text by newlines first
  String lines[20]; // Increased for scrolling
  int lineCount = 0;
  int lastIndex = 0;
  
  // Split by \n characters
  for (int i = 0; i <= text.length() && lineCount < 20; i++) {
    if (i == text.length() || text.charAt(i) == '\n') {
      lines[lineCount] = text.substring(lastIndex, i);
      lineCount++;
      lastIndex = i + 1;
    }
  }
  
  // Process each line with word wrapping
  String processedLines[30]; // Store all processed lines
  int processedLineCount = 0;
  
  for (int line = 0; line < lineCount; line++) {
    String lineText = lines[line];
    
    // If line is too long, break it into multiple lines
    while (lineText.length() > maxCharsPerLine) {
      String displayLine = lineText.substring(0, maxCharsPerLine);
      
      // Try to break at a space
      int breakPoint = displayLine.lastIndexOf(' ');
      if (breakPoint > maxCharsPerLine - 10) { // If space is not too far back
        displayLine = lineText.substring(0, breakPoint);
        lineText = lineText.substring(breakPoint + 1);
      } else {
        lineText = lineText.substring(maxCharsPerLine);
      }
      
      processedLines[processedLineCount] = displayLine;
      processedLineCount++;
    }
    
    // Add remaining part of line
    if (lineText.length() > 0) {
      processedLines[processedLineCount] = lineText;
      processedLineCount++;
    }
  }
  
  // If we have more lines than can fit on screen, implement scrolling
  if (processedLineCount > maxLines) {
    int scrollStart = 0;
    int scrollEnd = min(processedLineCount, maxLines);
    
    // Show initial content
    for (int i = scrollStart; i < scrollEnd; i++) {
      display.setCursor(0, contentStartY + (i - scrollStart) * 8);
      display.println(processedLines[i]);
    }
    display.display();
    delay(400); // Show initial content for 0.8 seconds (much faster)
    
    // Scroll through the content
    for (int scroll = 0; scroll <= processedLineCount - maxLines; scroll++) {
      display.clearDisplay();
      
      for (int i = scroll; i < scroll + maxLines && i < processedLineCount; i++) {
        display.setCursor(0, contentStartY + (i - scroll) * 8);
        display.println(processedLines[i]);
      }
      
      // Add scroll indicator
      if (scroll < processedLineCount - maxLines) {
        display.setCursor(120, SCREEN_HEIGHT - 8);
        display.print("▼");
      } else if (scroll > 0) {
        display.setCursor(120, SCREEN_HEIGHT - 8);
        display.print("▲");
      }
      
      // Add page indicator
      
      
      display.display();
      delay(500); // Show each screen for 1 second (much faster)
    }
  } else {
    // Content fits on screen, display normally
    for (int i = 0; i < processedLineCount; i++) {
      display.setCursor(0, contentStartY + i * 8);
      display.println(processedLines[i]);
    }
    display.display();
    delay(1500); // Show for 1.5 seconds (much faster)
  }
}

// Function to display text without status bar (for clean displays) - WITH SCROLLING
void displayTextClean(String text, int startY = 0) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  
  int y = startY;
  int maxCharsPerLine = 20; // Reduced to prevent overlapping
  int maxLines = (SCREEN_HEIGHT - startY) / 8;
  int currentLine = 0;
  
  // Split text by newlines first
  String lines[20]; // Increased for scrolling
  int lineCount = 0;
  int lastIndex = 0;
  
  // Split by \n characters
  for (int i = 0; i <= text.length() && lineCount < 20; i++) {
    if (i == text.length() || text.charAt(i) == '\n') {
      lines[lineCount] = text.substring(lastIndex, i);
      lineCount++;
      lastIndex = i + 1;
    }
  }
  
  // Process each line with word wrapping
  String processedLines[30]; // Store all processed lines
  int processedLineCount = 0;
  
  for (int line = 0; line < lineCount; line++) {
    String lineText = lines[line];
    
    // If line is too long, break it into multiple lines
    while (lineText.length() > maxCharsPerLine) {
      String displayLine = lineText.substring(0, maxCharsPerLine);
      
      // Try to break at a space
      int breakPoint = displayLine.lastIndexOf(' ');
      if (breakPoint > maxCharsPerLine - 10) { // If space is not too far back
        displayLine = lineText.substring(0, breakPoint);
        lineText = lineText.substring(breakPoint + 1);
      } else {
        lineText = lineText.substring(maxCharsPerLine);
      }
      
      processedLines[processedLineCount] = displayLine;
      processedLineCount++;
    }
    
    // Add remaining part of line
    if (lineText.length() > 0) {
      processedLines[processedLineCount] = lineText;
      processedLineCount++;enterLightSleep;
    }
  }
  
  // If we have more lines than can fit on screen, implement scrolling
  if (processedLineCount > maxLines) {
    int scrollStart = 0;
    int scrollEnd = min(processedLineCount, maxLines);
    
    // Show initial content
    for (int i = scrollStart; i < scrollEnd; i++) {
      display.setCursor(0, startY + (i - scrollStart) * 8);
      display.println(processedLines[i]);
    }
    display.display();
    delay(800); // Show initial content for 0.8 seconds (much faster)
    
    // Scroll through the content
    for (int scroll = 0; scroll <= processedLineCount - maxLines; scroll++) {
      display.clearDisplay();
      
      for (int i = scroll; i < scroll + maxLines && i < processedLineCount; i++) {
        display.setCursor(0, startY + (i - scroll) * 8);
        display.println(processedLines[i]);
      }
      
      // Add scroll indicator
      if (scroll < processedLineCount - maxLines) {
        display.setCursor(120, SCREEN_HEIGHT - 8);
        display.print("▼");
      } else if (scroll > 0) {
        display.setCursor(120, SCREEN_HEIGHT - 8);
        display.print("▲");
      }
      
      // Add page indicator
      display.setCursor(110, 0);
      display.print(String(scroll + 1) + "/" + String(processedLineCount - maxLines + 1));
      
      display.display();
      delay(1000); // Show each screen for 1 second (much faster)
    }
  } else {
    // Content fits on screen, display normally
    for (int i = 0; i < processedLineCount; i++) {
      display.setCursor(0, startY + i * 8);
      display.println(processedLines[i]);
    }
    display.display();
    delay(1500); // Show for 1.5 seconds (much faster)
  }
}

// Function to display AI analysis with interactive scrolling - NO STATUS BAR
void displayAIAnalysisWithScroll(String title, String category, String description) {
  // Limit description to 150 characters for faster scrolling
  String limitedDescription = description;
  if (limitedDescription.length() > 150) {
    limitedDescription = limitedDescription.substring(0, 147) + "...";
  }
  
  // Create formatted text for display
  String fullText = "AI ANALYSIS:\n";
  fullText += "Title: " + title + "\n";
  fullText += "Category: " + category + "\n";
  fullText += "\nDescription:\n" + limitedDescription;
  
  displayTextClean(fullText); // Use clean display without status bar
}

// Function to display status screen (Ready to scan) - WITH status bar
void displayStatusScreen() {
  display.clearDisplay();
  
  // ===== CHECK FOR FULL-SCREEN CHARGING ANIMATION =====
  if (showFullScreenCharging) {
    // Show full-screen charging animation for 3 seconds
    drawFullScreenCharging();
    display.display();
    return;
  }
  // ===== END CHARGING ANIMATION CHECK =====
  
  displayStatusBar(); // Show status bar
  display.setTextSize(2);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(20, 25); // Centered
  display.println("Ready");
  display.setTextSize(1);
  display.setCursor(30, 45); // Centered
  display.println("to scan");
  display.display();
}

// Function to display AI analysis process
void displayAIAnalysisProcess(String barcodeData) {
  // Step 1: Connecting to AI service
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  display.println("AI Analysis Process");
  display.println("==================");
  display.println("");
  display.println("Step 1: Connecting to");
  display.println("Gemini AI service...");
  display.display();
  delay(1500);
  
  // Step 2: Analyzing barcode
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  display.println("AI Analysis Process");
  display.println("==================");
  display.println("");
  display.println("Step 2: Analyzing");
  display.println("barcode data...");
  display.println("Barcode: " + barcodeData);
  display.display();
  delay(1500);
  
  // Step 3: Processing with AI
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  display.println("AI Analysis Process");
  display.println("==================");
  display.println("");
  display.println("Step 3: AI processing");
  display.println("product information...");
  display.println("");
  display.println("Please wait...");
  display.display();
  delay(2000);
  
  // Call Gemini API for analysis
  String aiResponse = callGeminiAPI(barcodeData);
  
  // Step 4: Display AI results
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  display.println("AI Analysis Complete");
  display.println("===================");
  display.println("");
  display.println("AI Response:");
  display.println(aiResponse.length() > 0 ? "Analysis received" : "No response");
  display.display();
  delay(3000);
  
  // If we got a response, show it
  if (aiResponse.length() > 0 && aiResponse != "WiFi not connected" && !aiResponse.startsWith("API Error")) {
    displayTextClean("AI Analysis Result:\n\n" + aiResponse);
    delay(3000); // Faster display
  } else {
    // Show error or fallback message
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    display.setCursor(0, 0);
    display.println("AI Analysis Failed");
    display.println("=================");
    display.println("");
    display.println("Reason: " + aiResponse);
    display.println("");
    display.println("Using fallback");
    display.println("identification...");
    display.display();
    delay(3000);
  }
}

String getCurrentTimestamp() {
  // Get current timestamp in ISO format
  // Note: This is a simplified version. For production, use proper time sync
  unsigned long currentTime = millis();
  return String(currentTime);
}

// Logo bitmap data
static const unsigned char PROGMEM logo16_glcd_bmp[] =
{
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x0f, 0xe0, 0x00, 0x00, 0xff, 0xc0, 0x00, 0x00, 0x78, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x0f, 0xf8, 0x00, 0x00, 0xff, 0xf8, 0x00, 0x00, 0x78, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x0f, 0xfc, 0x00, 0x00, 0xff, 0x3c, 0x00, 0x00, 0x78, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x0f, 0xbe, 0x00, 0x00, 0xff, 0x3e, 0x00, 0x00, 0x78, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x0f, 0xbe, 0x00, 0x00, 0xfe, 0x1e, 0x00, 0x00, 0x30, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x0f, 0x9f, 0x00, 0x00, 0xfe, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x0f, 0x9f, 0x00, 0x00, 0x06, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x0f, 0x1f, 0x00, 0x00, 0x06, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x0f, 0x1f, 0x80, 0x00, 0xfe, 0xff, 0x00, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x0e, 0x0f, 0x80, 0x00, 0xfc, 0xff, 0x00, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x0e, 0x0f, 0x80, 0x00, 0xfd, 0xff, 0x00, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x0e, 0x0f, 0x80, 0xf0, 0x01, 0xff, 0x06, 0x00, 0x70, 0x07, 0x0e, 0x00, 0x60, 0x00, 0x1c, 0x00, 
  0x0e, 0x07, 0x83, 0xfc, 0x03, 0xff, 0x07, 0x3c, 0x70, 0x1f, 0xce, 0x01, 0xf9, 0xc0, 0x7f, 0x00, 
  0x0e, 0x0f, 0x83, 0xfc, 0x7f, 0xff, 0x07, 0x7c, 0x70, 0x1f, 0xce, 0x03, 0xf9, 0xc0, 0x7f, 0x80, 
  0x0e, 0x8f, 0x87, 0xfe, 0x7f, 0xff, 0x07, 0x78, 0x70, 0x3f, 0xee, 0x07, 0xfd, 0xc0, 0xff, 0xc0, 
  0x0f, 0x2f, 0x8f, 0xff, 0x7f, 0xff, 0x07, 0xf8, 0x70, 0x3c, 0xfe, 0x07, 0x9d, 0xc1, 0xe3, 0xc0, 
  0x0f, 0xff, 0x0f, 0x0f, 0x3f, 0x9f, 0x07, 0xf8, 0x70, 0x78, 0x3e, 0x0f, 0x07, 0xc1, 0xc1, 0xe0, 
  0x0e, 0x0f, 0x1e, 0x07, 0x3f, 0x8e, 0x07, 0xc0, 0x70, 0x70, 0x1e, 0x0e, 0x07, 0xc3, 0x80, 0xe0, 
  0x0e, 0x0f, 0x1e, 0x07, 0xbf, 0x8e, 0x07, 0xc0, 0x70, 0x70, 0x1e, 0x1e, 0x03, 0xc3, 0x80, 0xe0, 
  0x0f, 0x0f, 0x1c, 0x03, 0x9f, 0x04, 0x07, 0x80, 0x70, 0xe0, 0x1e, 0x1c, 0x03, 0xc3, 0x80, 0x60, 
  0x0f, 0x1e, 0x1c, 0x03, 0x80, 0x06, 0x07, 0x00, 0x70, 0xe0, 0x0e, 0x1c, 0x01, 0xc3, 0x00, 0x70, 
  0x0f, 0x1e, 0x3c, 0x03, 0x9f, 0x0f, 0x07, 0x00, 0x70, 0xe0, 0x0e, 0x1c, 0x01, 0xc7, 0x00, 0x70, 
  0x0f, 0xfc, 0x38, 0x01, 0xdf, 0x8f, 0x07, 0x00, 0x70, 0xe0, 0x0e, 0x18, 0x01, 0xc7, 0x00, 0x70, 
  0x0f, 0x1c, 0x38, 0x01, 0xdf, 0x8f, 0x07, 0x00, 0x70, 0xe0, 0x0e, 0x18, 0x01, 0xc7, 0x00, 0x70, 
  0x0f, 0x9c, 0x38, 0x01, 0xdf, 0xff, 0x87, 0x00, 0x70, 0xe0, 0x0e, 0x18, 0x01, 0xc7, 0xff, 0xf0, 
  0x0f, 0x9c, 0x38, 0x01, 0xdf, 0xff, 0x87, 0x00, 0x70, 0xe0, 0x0e, 0x18, 0x01, 0xc7, 0xff, 0xf0, 
  0x0f, 0x9c, 0x38, 0x01, 0xdf, 0xff, 0x87, 0x00, 0x70, 0xe0, 0x0e, 0x18, 0x01, 0xc7, 0xff, 0xf0, 
  0x0f, 0x9c, 0x38, 0x01, 0xdf, 0xff, 0x87, 0x00, 0x70, 0xe0, 0x0e, 0x18, 0x01, 0xc7, 0x00, 0x00, 
  0x0f, 0x9e, 0x38, 0x01, 0x83, 0xff, 0x87, 0x00, 0x70, 0xe0, 0x0e, 0x1c, 0x01, 0xc7, 0x00, 0x00, 
  0x0f, 0x9e, 0x3c, 0x03, 0x81, 0xff, 0x87, 0x00, 0x70, 0xe0, 0x0e, 0x1c, 0x01, 0xc7, 0x00, 0x00, 
  0x0f, 0x9e, 0x1c, 0x03, 0x9c, 0xff, 0x87, 0x00, 0x70, 0xe0, 0x0e, 0x1c, 0x01, 0xc3, 0x00, 0x00, 
  0x0f, 0x9e, 0x1c, 0x03, 0x9e, 0xff, 0x87, 0x00, 0x70, 0xe0, 0x1e, 0x1c, 0x03, 0xc3, 0x80, 0x60, 
  0x0f, 0xbe, 0x1e, 0x07, 0xbe, 0x3f, 0x87, 0x00, 0x70, 0x70, 0x1e, 0x0e, 0x03, 0xc3, 0x80, 0xe0, 
  0x0f, 0xbf, 0x1e, 0x07, 0x26, 0x3f, 0x07, 0x00, 0x70, 0x70, 0x1e, 0x0e, 0x07, 0xc3, 0xc0, 0xe0, 
  0x0f, 0x9f, 0x0f, 0x0f, 0x06, 0x1f, 0x07, 0x00, 0x70, 0x78, 0x3e, 0x0f, 0x07, 0xc1, 0xc1, 0xe0, 
  0x0f, 0x9f, 0x0f, 0xff, 0x06, 0x1f, 0x07, 0x00, 0x70, 0x3c, 0x6e, 0x07, 0x9d, 0xc1, 0xf7, 0xc0, 
  0x0f, 0x9f, 0x07, 0xfe, 0x06, 0x3e, 0x07, 0x00, 0x70, 0x3f, 0xee, 0x07, 0xfd, 0xc0, 0xff, 0x80, 
  0x0f, 0x9f, 0x83, 0xfc, 0xc7, 0x3e, 0x07, 0x00, 0x70, 0x1f, 0xce, 0x03, 0xf9, 0xc0, 0xff, 0x80, 
  0x0f, 0x9f, 0x83, 0xf8, 0xe7, 0xfc, 0x07, 0x00, 0x70, 0x0f, 0xce, 0x01, 0xf1, 0xc0, 0x3f, 0x00, 
  0x00, 0x00, 0x00, 0xf0, 0xef, 0xe0, 0x06, 0x00, 0x00, 0x07, 0x00, 0x00, 0x41, 0xc0, 0x1c, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xc0, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x03, 0x80, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0e, 0x03, 0x80, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0e, 0x07, 0x80, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x0f, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0xff, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0xfe, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xfc, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xfc, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

void setup() {
  Serial.begin(9600);
  delay(1000); // Give serial time to initialize
  
  debugPrint("=== ESP32 GM77 Barcode Scanner Starting ===");
  debugPrint("Firmware Version: " + String(firmwareVersion));
  debugPrint("Device ID: " + String(deviceId));
  debugPrint("Device Name: " + String(deviceName));

  // Init GM77 scanner (baud: 9600, RX=16, TX=17)
  debugPrint("Initializing GM77 barcode scanner...");
  GM77.begin(9600, SERIAL_8N1, 16, 17);
  debugPrint("GM77 scanner initialized on UART2 (GPIO16 RX, GPIO17 TX)");

  // ===== CRITICAL: Init OLED FIRST (before GPIO wake and MAX17043) =====
  // This order is required for custom PCB compatibility
  debugPrint("Initializing OLED display...");
  debugPrint("Trying I2C address 0x3C...");
  if (!display.begin(0x3C, true)) {
    debugPrint("0x3C failed, trying 0x3D...");
    if (!display.begin(0x3D, true)) {
      debugPrint("ERROR: OLED init failed on both 0x3C and 0x3D!");
      debugPrint("Troubleshooting:");
      debugPrint("  1. Check wiring: SDA=GPIO21, SCL=GPIO22");
      debugPrint("  2. Verify 3.3V power (NOT 5V!)");
      debugPrint("  3. Run I2C_Scanner.ino to detect devices");
      debugPrint("  4. Check DISPLAY_TROUBLESHOOTING.md");
      for (;;);
    } else {
      debugPrint("OLED display initialized successfully at 0x3D");
    }
  } else {
    debugPrint("OLED display initialized successfully at 0x3C");
  }
  
  // CRITICAL FIX: Clear display buffer immediately after initialization
  // This prevents random garbage/noise from appearing on screen during boot
  display.clearDisplay();
  display.display();
  debugPrint("Display buffer cleared - ready for use");



  // Init MAX1704X Fuel Gauge FIRST (needed even when locked for battery monitoring)
  debugPrint("Initializing MAX1704X Fuel Gauge...");
  
  fuelGauge.begin(DEFER_ADDRESS);
  uint8_t addr = fuelGauge.findFirstDevice();
  
  if (addr == 0) {
    debugPrint("ERROR: MAX1704X NOT FOUND!");
    debugPrint("Check I2C connections and power");
    // Set default values
    voltage = 0.0;
    bat_percentage = 0.0;
  } else {
    debugPrint("MAX1704X found at address: 0x" + String(addr, HEX));
    fuelGauge.address(addr);
    fuelGauge.reset();
    delay(250);
    fuelGauge.quickstart();
    delay(150);
    debugPrint("MAX1704X initialized successfully");
  }

  // ===== CONFIGURE CHARGING DETECTION PIN =====
  debugPrint("Configuring charging detection pin...");
  pinMode(CHARGING_PIN, INPUT_PULLUP);  // CHRG pin from charger IC (active LOW)
  debugPrint("Charging detection pin configured (GPIO 27: CHRG)");

  // Light Sleep GPIO Wake Configuration - Wake on trigger button press
  debugPrint("Configuring GPIO wake-up for light sleep...");
  pinMode(GM77_TRIG_PIN, INPUT_PULLUP);  // Configure trigger pin with pull-up
  gpio_wakeup_enable((gpio_num_t)GM77_TRIG_PIN, GPIO_INTR_LOW_LEVEL);  // Wake on trigger press (LOW)
  esp_sleep_enable_gpio_wakeup();
  lastActivityTime = millis();
  debugPrint("Wake-on-trigger configured (GPIO 35) - Press trigger to wake from sleep");

  // ===== CHECK SYSTEM LOCK STATE =====
  // Do this AFTER hardware initialization but BEFORE showing any screens
  debugPrint("Checking system lock state...");
  
  // TEMPORARY: Force reset to locked state (comment out after first boot)
  // resetLockState();
  
  loadLockState();
  
  if (systemLocked) {
    // System is locked - skip logo, WiFi, battery warnings, and server registration
    debugPrint("System is LOCKED - waiting for initialization barcode");
    displayLockedScreen();
    debugPrint("=== System in Locked State - Scan " + INIT_BARCODE + " to unlock ===");
    return;  // Exit setup early, skip all initialization
  }
  // ===== END SYSTEM LOCK CHECK =====

  // ===== SYSTEM IS UNLOCKED - PROCEED WITH NORMAL INITIALIZATION =====
  debugPrint("System is UNLOCKED - proceeding with normal initialization");


  // Show logo
  debugPrint("Displaying startup logo...");
  display.clearDisplay();
  display.drawBitmap(0, 0, epd_bitmap_ro_bridge, 128, 64, 1);
  display.display();
  delay(3000);
  
  // Initialize WiFi variables
  debugPrint("Initializing WiFi variables...");
  lastWiFiCheck = 0;
  reconnectAttempts = 0;
  wifiReconnectInProgress = false;
  wifiConnected = false;
  robridgeConnected = false;
  isRegistered = false;
  
  // Connect to WiFi and register with Robridge
  debugPrint("Starting WiFi connection process...");
  connectWiFi();
  
  // Show ready message
  debugPrint("System initialization complete. Showing status screen...");
  delay(500);  // Small delay to ensure previous display update completes
  displayStatusScreen();
  
  debugPrint("=== System Ready ===");
  debugPrint("Available debug commands: wifi_status, wifi_reconnect, wifi_scan, help");
}

void loop() {
  // ===== LOOP HEARTBEAT (Debug) =====
  static unsigned long lastHeartbeat = 0;
  if (millis() - lastHeartbeat > 5000) {
    Serial.println("💓 Loop running... (heartbeat every 5s)");
    lastHeartbeat = millis();
  }
  
  // Update battery reading every second (works in both locked and unlocked states)
  updateBattery();
  
  // ===== CHECK FOR 10-SECOND TRIGGER HOLD (MANUAL RESTART) - WORKS IN ANY STATE =====
  static unsigned long triggerPressStart = 0;
  static bool triggerHoldDetected = false;
  
  // Read trigger button state
  int buttonState = digitalRead(GM77_TRIG_PIN);
  
  if (buttonState == LOW) {  // Trigger button pressed (active LOW)
    if (triggerPressStart == 0) {
      triggerPressStart = millis();  // Start timing
      triggerHoldDetected = false;
      Serial.println("🔘 Trigger button PRESSED - hold for 10 seconds to restart");
      Serial.print("   Button state: ");
      Serial.println(buttonState);
    }
    
    unsigned long holdDuration = millis() - triggerPressStart;
    
    // Print status every 2 seconds while holding
    static unsigned long lastStatusPrint = 0;
    if (millis() - lastStatusPrint > 2000) {
      Serial.print("⏱️  Holding for ");
      Serial.print(holdDuration / 1000);
      Serial.println(" seconds...");
      lastStatusPrint = millis();
    }
    
    // Check if held for 10 seconds
    if (holdDuration >= 10000 && !triggerHoldDetected) {
      triggerHoldDetected = true;
      
      Serial.println("✅ 10 SECONDS REACHED - RESTARTING NOW!");
      
      // Show restart message
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SH110X_WHITE);
      display.setCursor(0, 20);
      display.println("Scanner Restarting...");
      display.println("");
      display.println("Please wait...");
      display.display();
      delay(2000);
      
      // Perform restart
      debugPrint("Manual restart triggered by 10-second button hold");
      ESP.restart();
    }
  } else {
    // Button released - reset timer
    if (triggerPressStart != 0) {
      Serial.println("🔘 Trigger button RELEASED - restart cancelled");
      Serial.print("   Button state: ");
      Serial.println(buttonState);
    }
    triggerPressStart = 0;
    triggerHoldDetected = false;
  }
  // ===== END TRIGGER HOLD CHECK =====
  
  // ===== SYSTEM LOCK CHECK - HIGHEST PRIORITY =====
  if (systemLocked) {
    // System is locked - only check for initialization barcode
    if (GM77.available()) {
      lastActivityTime = millis();
      
      // Read barcode data
      String rawData = GM77.readStringUntil('\n');
      String barcodeData = cleanBarcode(rawData);
      
      if (barcodeData.length() > 0) {
        debugPrint("Barcode scanned while locked: " + barcodeData);
        
        // Check if it's the initialization barcode
        if (barcodeData == INIT_BARCODE) {
          debugPrint("✅ INITIALIZATION BARCODE DETECTED!");
          unlockSystem();  // This will initialize WiFi, show logo, etc.
        } else {
          // Wrong barcode - show error message
          debugPrint("❌ Invalid barcode - system remains locked");
          display.clearDisplay();
          display.setTextSize(1);
          display.setTextColor(SH110X_WHITE);
          display.setCursor(0, 10);
          display.println("  Invalid Code!");
          display.println("");
          display.println("  Scan Init Code");
          display.println("  from Guide");
          display.display();
          delay(2000);
          
          // Return to locked screen
          displayLockedScreen();
        }
        
        // Flush remaining data
        while (GM77.available()) {
          GM77.read();
        }
      }
    }
    
    // ===== CHECK FOR SLEEP IN LOCKED STATE =====
    unsigned long currentTime = millis();
    if (displayOn && (currentTime - lastActivityTime > SLEEP_TIMEOUT)) {
      Serial.println("Locked screen idle - entering light sleep...");
      enterLightSleep();
    }
    // ===== END SLEEP CHECK =====
    
    // Small delay to prevent watchdog issues
    delay(10);
    return;  // Skip all other loop operations when locked
  }
  
  // ===== SYSTEM IS UNLOCKED - NORMAL OPERATION =====
  
  // Debug: Monitor sleep timer every 2 seconds
  static unsigned long lastDebug = 0;
  if (millis() - lastDebug > 2000) {
    unsigned long idleTime = millis() - lastActivityTime;
    Serial.printf("Idle time: %lu ms / %d ms (Display: %s)\n", 
                  idleTime, SLEEP_TIMEOUT, displayOn ? "ON" : "OFF");
    lastDebug = millis();
  }
  
  // Enhanced WiFi monitoring and auto-reconnect
  checkWiFiConnection();
  
  // Check for debug commands via Serial
  if (Serial.available()) {
    lastActivityTime = millis(); // UPDATE: Activity detected
    
    String command = Serial.readStringUntil('\n');
    command.trim();
    
    if (command == "wifi_status") {
      debugPrintWiFiStatus();
    } else if (command == "wifi_reconnect") {
      debugPrint("Manual WiFi reconnect requested...");
      attemptWiFiReconnect();
    } else if (command == "wifi_scan") {
      debugPrint("Scanning for available networks...");
      int networks = WiFi.scanNetworks();
      debugPrint("Found " + String(networks) + " networks:");
      for (int i = 0; i < networks; i++) {
        debugPrint("  " + String(i+1) + ": " + WiFi.SSID(i) + " (RSSI: " + String(WiFi.RSSI(i)) + " dBm)");
      }
    } else if (command == "test_server") {
      debugPrint("Testing server connection...");
      Product testProduct = analyzeProductWithAI("123456789");
      debugPrint("Test completed");
    } else if (command == "register") {
      debugPrint("Manually registering with Robridge server...");
      registerWithRobridge();
      debugPrint("Registration attempt completed");
    } else if (command == "server_config") {
      debugPrint("=== Server Configuration ===");
      debugPrint("Custom Server IP: " + (customServerIP.length() > 0 ? customServerIP : "Not set"));
      debugPrint("Express Server URL: " + expressServerURL);
      debugPrint("AI Server URL: " + aiServerURL);
      debugPrint("========================");
    } else if (command == "reset_config") {
      debugPrint("Resetting server configuration...");
      customServerIP = "";
      saveServerConfig();
      updateServerURLs();
      debugPrint("Server configuration reset to default cloud URLs");
    } else if (command == "reset_lock") {
      debugPrint("Resetting system lock state to LOCKED...");
      resetLockState();
      debugPrint("System lock state reset. Device will restart...");
      delay(2000);
      ESP.restart();
    } else if (command == "help") {
      debugPrint("Available commands:");
      debugPrint("  wifi_status - Show detailed WiFi status");
      debugPrint("  wifi_reconnect - Force WiFi reconnection");
      debugPrint("  wifi_scan - Scan for available networks");
      debugPrint("  test_server - Test server connection");
      debugPrint("  register - Manually register with Robridge");
      debugPrint("  server_config - Show current server configuration");
      debugPrint("  reset_config - Reset to default cloud servers");
      debugPrint("  reset_lock - Reset system to locked state (requires init barcode)");
      debugPrint("  help - Show this help message");
    }
  }
  
  // Send periodic ping to Robridge server (only if connected)
  if (wifiConnected && millis() - lastPingTime > pingInterval) {
    debugPrint("Sending periodic ping to Robridge server...");
    sendPingToRobridge();
    lastPingTime = millis();
  }
  
  // Check for barcode scan
  if (GM77.available()) {
    lastActivityTime = millis(); // UPDATE: Activity detected
    
    // Read until newline
    String rawData = GM77.readStringUntil('\n');
    String barcodeData = cleanBarcode(rawData);

    if (barcodeData.length() > 0) {
      // ===== DUPLICATE SCAN PREVENTION =====
      // Ignore duplicate scans within 3 seconds (for continuous mode)
      unsigned long currentTime = millis();
      if (barcodeData == lastScannedCode && (currentTime - lastScanTime) < 3000) {
        debugPrint("⚠️ Duplicate scan ignored: " + barcodeData);
        // Flush remaining data and return
        while (GM77.available()) {
          GM77.read();
        }
        return;
      }
      
      // Update last scan info
      lastScannedCode = barcodeData;
      lastScanTime = currentTime;
      // ===== END DUPLICATE PREVENTION =====
      
      // Print clean data to serial
      Serial.print("Clean Barcode: ");
      Serial.println(barcodeData);

      // ===== CHECK FOR PAIRING QR CODE FIRST =====
      if (barcodeData.startsWith("ROBRIDGE_PAIR|")) {
        debugPrint("🔔 Pairing QR code detected!");
        
        // Show scanning message
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SH110X_WHITE);
        display.setCursor(0, 0);
        display.println("Pairing QR Detected");
        display.println("");
        display.println("Processing...");
        display.display();
        delay(1000);
        
        // Parse and pair
        if (parsePairingQR(barcodeData)) {
          pairDeviceWithUser();
        } else {
          display.clearDisplay();
          display.setTextSize(1);
          display.setTextColor(SH110X_WHITE);
          display.setCursor(0, 0);
          display.println("Invalid QR Code");
          display.println("");
          display.println("Please scan a");
          display.println("valid pairing QR");
          display.display();
          delay(3000);
        }
        
        // Don't process as regular barcode
        return;
      }
      // ===== END PAIRING QR CHECK =====

      // Check if device name contains "AI" for AI analysis
      bool hasAI = deviceName.indexOf("AI") >= 0;
      debugPrint("Device name: " + deviceName);
      debugPrint("Has AI capability: " + String(hasAI ? "YES" : "NO"));
      
      if (hasAI) {
        // Send to Robridge server IMMEDIATELY (before AI analysis delays)
        // This ensures instant response even if AI takes time
        if (isPaired) {
          // Send basic scan first for instant web display
          sendBasicScanToRobridge(barcodeData);
        }
        
        // Now do AI analysis (this won't delay the server response)
        // Show AI Analysis message briefly
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SH110X_WHITE);
        display.setCursor(0, 0);
        display.println("AI Analysis...");
        display.println("");
        display.println("Processing barcode:");
        display.println(barcodeData);
        display.display();
        delay(4000);

        // Direct AI analysis
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SH110X_WHITE);
        display.setCursor(0, 0);
        display.println("Analyzing with AI...");
        display.println("Barcode: " + barcodeData);
        display.display();
        delay(4000);
        
        // Analyze with AI model directly
        Product aiProduct = analyzeProductWithAI(barcodeData);
        
        if (aiProduct.name.length() > 0) {
          // AI successfully analyzed the product
          displayAIAnalysisWithScroll(aiProduct.name, aiProduct.category, aiProduct.details);
          
          // Update the scan with AI product info (optional - for enrichment)
          if (isPaired) {
            sendScanToRobridge(barcodeData, &aiProduct);
          }
        } else {
          // AI analysis failed - show basic info
          displayBasicScanInfo(barcodeData);
        }
      } else {
        // Device doesn't have "AI" in name
        // Show on display INSTANTLY with all info
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SH110X_WHITE);
        display.setCursor(0, 0);
        display.println("Scanned:");
        display.println(barcodeData);
        display.println("");
        display.println("Device: " + deviceName);
        display.display();
        
        // Send to server immediately (happens in background)
        if (isPaired) {
          sendBasicScanToRobridge(barcodeData);
        }
        
        // Keep display showing for 4 seconds
        delay(4000);
        
        // Return to ready state
        displayStatusBar();
        display.setCursor(0, 20);
        display.setTextSize(1);
        display.println("Ready to scan");
        display.display();
      }

      // Flush remaining data
      while (GM77.available()) {
        GM77.read();
      }
      
      // Return to status screen
      displayStatusScreen();
      lastActivityTime = millis(); // UPDATE: Reset timer after scan
    }
  }
  
  // ===== PERIODIC DISPLAY UPDATE FOR CHARGING ANIMATION =====
  // Update display periodically when charging animation is active
  static unsigned long lastDisplayUpdate = 0;
  if (showFullScreenCharging && (millis() - lastDisplayUpdate > 100)) {
    displayStatusScreen();  // This will show the animated charging screen
    lastDisplayUpdate = millis();
  }
  
  // Update status bar when charging (for animated battery icon)
  if (isCharging && !showFullScreenCharging && (millis() - lastDisplayUpdate > 200)) {
    displayStatusScreen();  // Refresh to show animated charging icon
    lastDisplayUpdate = millis();
  }
  // ===== END PERIODIC DISPLAY UPDATE =====
  
  // --- CRITICAL: Light Sleep Check (ONLY ONE, AT THE END) ---
  unsigned long currentTime = millis();
  if (displayOn && (currentTime - lastActivityTime > SLEEP_TIMEOUT)) {
    Serial.println("Idle too long - entering light sleep...");
    enterLightSleep();
  }
  
  // Small delay to prevent watchdog issues
  delay(10);
}

// Function to display basic scan info without AI analysis
void displayBasicScanInfo(String barcodeData) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  display.println("Barcode: " + barcodeData);
  display.println("");
  display.println("Device: " + deviceName);
  display.println("processing");
  display.display();
  delay(4000);
}

// Function to send basic scan data to Robridge server (without AI analysis)
void sendBasicScanToRobridge(String barcodeData) {
  if (!wifiConnected) {
    debugPrint("WiFi not connected, skipping basic scan send");
    return;
  }
  
  if (!isPaired) {
    debugPrint("Device not paired, skipping basic scan send");
    return;
  }

  String serverUrl = expressServerURL + "/api/esp32/scan/" + deviceId;
  
  // Create JSON payload for basic scan (no AI analysis)
  String jsonString = "{";
  jsonString += "\"barcodeData\":\"" + barcodeData + "\",";
  jsonString += "\"scanType\":\"basic_scan\",";
  jsonString += "\"timestamp\":" + String(millis()) + ",";
  jsonString += "\"source\":\"esp32_basic\",";
  jsonString += "\"productName\":\"Unknown Product\",";
  jsonString += "\"productType\":\"Unknown\",";
  jsonString += "\"productDetails\":\"Basic scan without AI analysis\",";
  jsonString += "\"productCategory\":\"Unknown\"";
  jsonString += "}";

  debugPrint("Sending basic scan to Robridge: " + jsonString);

  HTTPClient http;
  http.begin(serverUrl);
  http.addHeader("Content-Type", "application/json");
  
  int httpResponseCode = http.POST(jsonString);
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    debugPrint("Basic scan response: " + String(httpResponseCode) + " - " + response);
    lastApiResponse = response;
  } else {
    debugPrint("Basic scan failed: " + String(httpResponseCode));
  }
  
  http.end();
}

// ---------------------------------------------------------------
// Light Sleep Helper Functions
// ---------------------------------------------------------------
void enterLightSleep() {
  display.clearDisplay();
  display.display();
  display.oled_command(SH110X_DISPLAYOFF);
  displayOn = false;
  Serial.println("Display OFF - Going to light sleep...");
  Serial.flush();

  while (GM77.available()) GM77.read();
  sleepStartTime = millis();

  esp_light_sleep_start();

  unsigned long sleepDuration = millis() - sleepStartTime;
  Serial.printf("Woke up from light sleep after %lu ms\n", sleepDuration);

  delay(100);
  
  // Quick wake from light sleep
  Serial.println("Waking from light sleep");
  wakeDisplay();  // Instant wake (~100ms)
  
  lastActivityTime = millis();
}

void wakeDisplay() {
  display.oled_command(SH110X_DISPLAYON);
  displayOn = true;
  Serial.println("Display ON - resumed");
  
  // Show appropriate screen based on system lock state
  if (systemLocked) {
    displayLockedScreen();
  } else {
    displayStatusScreen();
  }
}



void wakeDisplayInitial() {
  display.oled_command(SH110X_DISPLAYON);
  displayOn = true;
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.println("ROBRIDGE System");
  display.println("Reinitializing...");
  display.display();
  delay(2000);
  Serial.println("Display ON - initial state reset");
}
