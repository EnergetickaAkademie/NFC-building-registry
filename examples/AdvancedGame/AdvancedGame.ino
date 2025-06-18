#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <NFCBuildingRegistry.h>

// MFRC522 pin definitions
#ifdef ESP8266
  #define SS_PIN    D8   // SDA pin
  #define RST_PIN   D3   // RST pin
#elif defined(ESP32)
  #define SS_PIN    21   // SDA pin
  #define RST_PIN   22   // RST pin
#endif

// Building type definitions
#define BUILDING_TYPE_HOUSE       1
#define BUILDING_TYPE_APARTMENT   2
#define BUILDING_TYPE_OFFICE      3
#define BUILDING_TYPE_SHOP        4
#define BUILDING_TYPE_FACTORY     5
#define BUILDING_TYPE_WAREHOUSE   6
#define BUILDING_TYPE_SCHOOL      7
#define BUILDING_TYPE_HOSPITAL    8

MFRC522 mfrc522(SS_PIN, RST_PIN);
NFCBuildingRegistry buildingRegistry(&mfrc522);

// Game state
unsigned long gameStartTime;
bool gameActive = false;
uint8_t targetBuildingTypes[] = {BUILDING_TYPE_HOUSE, BUILDING_TYPE_SHOP, BUILDING_TYPE_OFFICE};
int targetCount = 3;

void onNewBuilding(uint8_t buildingType, const String& uid) {
  Serial.println("🏢 Building Added: " + getBuildingTypeName(buildingType) + " (UID: " + uid + ")");
  
  if (gameActive) {
    checkGameProgress();
  }
}

void onDeleteBuilding(uint8_t buildingType, const String& uid) {
  Serial.println("🗑️ Building Removed: " + getBuildingTypeName(buildingType) + " (UID: " + uid + ")");
}

String getBuildingTypeName(uint8_t type) {
  switch (type) {
    case BUILDING_TYPE_HOUSE:     return "House";
    case BUILDING_TYPE_APARTMENT: return "Apartment";
    case BUILDING_TYPE_OFFICE:    return "Office";
    case BUILDING_TYPE_SHOP:      return "Shop";
    case BUILDING_TYPE_FACTORY:   return "Factory";
    case BUILDING_TYPE_WAREHOUSE: return "Warehouse";
    case BUILDING_TYPE_SCHOOL:    return "School";
    case BUILDING_TYPE_HOSPITAL:  return "Hospital";
    default:                      return "Unknown";
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("=== Advanced Building Registry Game ===");
  
  SPI.begin();
  mfrc522.PCD_Init();
  mfrc522.PCD_DumpVersionToSerial();
  
  buildingRegistry.setOnNewBuildingCallback(onNewBuilding);
  buildingRegistry.setOnDeleteBuildingCallback(onDeleteBuilding);
  
  Serial.println();
  Serial.println("Commands:");
  Serial.println("  'start' - Start collection game");
  Serial.println("  'stop'  - Stop game");
  Serial.println("  'delete' - Toggle delete mode");
  Serial.println("  'clear' - Clear all buildings");
  Serial.println("  'show' - Show all buildings");
  Serial.println("  'stats' - Show statistics");
  Serial.println("  'filter <type>' - Show buildings of specific type (1-8)");
  Serial.println();
  Serial.println("Ready! Place NFC cards near the reader...");
  Serial.println();
}

void loop() {
  handleSerialCommands();
  buildingRegistry.scanForCards();
  delay(100);
}

void handleSerialCommands() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    
    if (command.startsWith("start")) {
      startGame();
    }
    else if (command.startsWith("stop")) {
      stopGame();
    }
    else if (command.startsWith("delete")) {
      buildingRegistry.setDeleteMode(!buildingRegistry.isDeleteMode());
      Serial.println("Delete mode: " + String(buildingRegistry.isDeleteMode() ? "ON" : "OFF"));
    }
    else if (command.startsWith("clear")) {
      buildingRegistry.clearDatabase();
      Serial.println("All buildings cleared!");
    }
    else if (command.startsWith("show")) {
      showAllBuildings();
    }
    else if (command.startsWith("stats")) {
      showDetailedStats();
    }
    else if (command.startsWith("filter ")) {
      int type = command.substring(7).toInt();
      if (type >= 1 && type <= 8) {
        showBuildingsByType(type);
      } else {
        Serial.println("Invalid building type. Use 1-8.");
      }
    }
    else if (command.length() > 0) {
      Serial.println("Unknown command: " + command);
    }
    Serial.println();
  }
}

void startGame() {
  buildingRegistry.clearDatabase();
  buildingRegistry.setDeleteMode(false);
  gameActive = true;
  gameStartTime = millis();
  
  Serial.println("🎮 COLLECTION GAME STARTED!");
  Serial.println("Collect these building types:");
  for (int i = 0; i < targetCount; i++) {
    Serial.println("  - " + getBuildingTypeName(targetBuildingTypes[i]));
  }
  Serial.println("Scan cards to collect buildings!");
  Serial.println();
}

void stopGame() {
  gameActive = false;
  Serial.println("🛑 Game stopped.");
}

void checkGameProgress() {
  bool allFound = true;
  Serial.println("📊 Progress check:");
  
  for (int i = 0; i < targetCount; i++) {
    uint8_t type = targetBuildingTypes[i];
    bool hasType = buildingRegistry.hasBuildingType(type);
    Serial.println("  " + getBuildingTypeName(type) + ": " + (hasType ? "✅" : "❌"));
    if (!hasType) allFound = false;
  }
  
  if (allFound) {
    unsigned long gameTime = millis() - gameStartTime;
    Serial.println();
    Serial.println("🎉 CONGRATULATIONS! You collected all required buildings!");
    Serial.println("⏱️ Time: " + String(gameTime / 1000) + " seconds");
    Serial.println("🏢 Total buildings collected: " + String(buildingRegistry.getDatabaseSize()));
    gameActive = false;
  }
  Serial.println();
}

void showAllBuildings() {
  Serial.println("=== All Buildings ===");
  auto buildings = buildingRegistry.getAllBuildings();
  
  if (buildings.empty()) {
    Serial.println("No buildings registered.");
    return;
  }
  
  for (const auto& pair : buildings) {
    const BuildingCard& card = pair.second;
    Serial.println("🏢 " + getBuildingTypeName(card.buildingType) + 
                   " | UID: " + card.uid + 
                   " | Age: " + String((millis() - card.firstSeen) / 1000) + "s");
  }
  Serial.println("Total: " + String(buildings.size()) + " buildings");
}

void showDetailedStats() {
  Serial.println("=== Detailed Statistics ===");
  Serial.println("Total buildings: " + String(buildingRegistry.getDatabaseSize()));
  Serial.println("Delete mode: " + String(buildingRegistry.isDeleteMode() ? "ON" : "OFF"));
  Serial.println("Game active: " + String(gameActive ? "YES" : "NO"));
  
  Serial.println("\nBuildings by type:");
  for (int type = 1; type <= 8; type++) {
    size_t count = buildingRegistry.getBuildingCount(type);
    if (count > 0) {
      Serial.println("  " + getBuildingTypeName(type) + ": " + String(count));
    }
  }
  
  if (gameActive) {
    Serial.println("\nGame progress:");
    for (int i = 0; i < targetCount; i++) {
      uint8_t type = targetBuildingTypes[i];
      bool hasType = buildingRegistry.hasBuildingType(type);
      Serial.println("  " + getBuildingTypeName(type) + ": " + (hasType ? "Found" : "Missing"));
    }
  }
}

void showBuildingsByType(uint8_t buildingType) {
  Serial.println("=== " + getBuildingTypeName(buildingType) + " Buildings ===");
  
  auto buildings = buildingRegistry.getBuildingsByType(buildingType);
  
  if (buildings.empty()) {
    Serial.println("No " + getBuildingTypeName(buildingType) + " buildings found.");
    return;
  }
  
  for (const auto& pair : buildings) {
    const BuildingCard* card = pair.second;
    Serial.println("🏢 UID: " + card->uid + 
                   " | First seen: " + String((millis() - card->firstSeen) / 1000) + "s ago" +
                   " | Last seen: " + String((millis() - card->lastSeen) / 1000) + "s ago");
  }
  Serial.println("Total: " + String(buildings.size()) + " buildings");
}
