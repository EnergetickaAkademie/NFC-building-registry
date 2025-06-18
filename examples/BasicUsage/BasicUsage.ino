#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <NFCBuildingRegistry.h>

// MFRC522 pin definitions for Wemos D1 Mini
#define SS_PIN    D8   // SDA pin
#define RST_PIN   D3   // RST pin

// For ESP32, use different pins:
// #define SS_PIN    21   // SDA pin
// #define RST_PIN   22   // RST pin

// Create MFRC522 instance
MFRC522 mfrc522(SS_PIN, RST_PIN);

// Create building registry
NFCBuildingRegistry buildingRegistry(&mfrc522);

// Callback functions
void onNewBuilding(uint8_t buildingType, const String& uid) {
  Serial.println("🏢 NEW BUILDING DETECTED!");
  Serial.println("   Type: " + String(buildingType));
  Serial.println("   UID: " + uid);
  Serial.println("   Total buildings: " + String(buildingRegistry.getDatabaseSize()));
  Serial.println();
}

void onDeleteBuilding(uint8_t buildingType, const String& uid) {
  Serial.println("🗑️ BUILDING DELETED!");
  Serial.println("   Type: " + String(buildingType));
  Serial.println("   UID: " + uid);
  Serial.println("   Remaining buildings: " + String(buildingRegistry.getDatabaseSize()));
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("=== NFC Building Registry Example ===");
  
  // Initialize SPI bus
  SPI.begin();
  
  // Initialize MFRC522
  mfrc522.PCD_Init();
  mfrc522.PCD_DumpVersionToSerial();
  
  // Set up callbacks
  buildingRegistry.setOnNewBuildingCallback(onNewBuilding);
  buildingRegistry.setOnDeleteBuildingCallback(onDeleteBuilding);
  
  Serial.println();
  Serial.println("Commands:");
  Serial.println("  'd' - Toggle delete mode");
  Serial.println("  'c' - Clear database");
  Serial.println("  'p' - Print database");
  Serial.println("  's' - Show statistics");
  Serial.println();
  Serial.println("Ready! Place NFC cards near the reader...");
  Serial.println("Delete mode: " + String(buildingRegistry.isDeleteMode() ? "ON" : "OFF"));
  Serial.println();
}

void loop() {
  // Handle serial commands
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    command.toLowerCase();
    
    if (command == "d") {
      // Toggle delete mode
      buildingRegistry.setDeleteMode(!buildingRegistry.isDeleteMode());
      Serial.println("Delete mode: " + String(buildingRegistry.isDeleteMode() ? "ON" : "OFF"));
    }
    else if (command == "c") {
      // Clear database
      buildingRegistry.clearDatabase();
      Serial.println("Database cleared!");
    }
    else if (command == "p") {
      // Print database
      buildingRegistry.printDatabase();
    }
    else if (command == "s") {
      // Show statistics
      showStatistics();
    }
    else {
      Serial.println("Unknown command: " + command);
    }
    Serial.println();
  }
  
  // Scan for cards
  buildingRegistry.scanForCards();
  
  delay(100); // Small delay to prevent flooding
}

void showStatistics() {
  Serial.println("=== Building Statistics ===");
  Serial.println("Total buildings: " + String(buildingRegistry.getDatabaseSize()));
  
  // Count buildings by type
  std::map<uint8_t, int> typeCounts;
  auto allBuildings = buildingRegistry.getAllBuildings();
  
  for (const auto& pair : allBuildings) {
    uint8_t type = pair.second.buildingType;
    typeCounts[type]++;
  }
  
  Serial.println("Buildings by type:");
  for (const auto& pair : typeCounts) {
    Serial.println("  Type " + String(pair.first) + ": " + String(pair.second) + " buildings");
  }
  
  Serial.println("===========================");
}
