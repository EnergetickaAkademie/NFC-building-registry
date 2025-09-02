#ifndef NFC_BUILDING_REGISTRY_H
#define NFC_BUILDING_REGISTRY_H

#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>

#if defined(ESP8266)
  #include <map>
  #include <functional>
  #include <vector>
#elif defined(ESP32)
  #include <map>
  #include <functional>
  #include <vector>
  #include <mutex>
#else
  #error "This library only supports ESP8266 and ESP32"
#endif

// Structure to represent a building card
struct BuildingCard {
  String uid;           // Card UID as hex string
  uint8_t buildingType; // Building type (8-bit)
  unsigned long firstSeen; // Timestamp when first registered
  unsigned long lastSeen;  // Timestamp when last seen
  
  BuildingCard() : uid(""), buildingType(0), firstSeen(0), lastSeen(0) {}
  BuildingCard(const String& cardUid, uint8_t type) 
    : uid(cardUid), buildingType(type), firstSeen(millis()), lastSeen(millis()) {}
};

// Callback function type for new building events
typedef std::function<void(uint8_t buildingType, const String& uid)> BuildingEventCallback;

class NFCBuildingRegistry {
private:
  MFRC522* mfrc522;
  std::map<String, BuildingCard> buildingDatabase; // UID -> BuildingCard mapping (protected by dbMutex)
  mutable std::mutex dbMutex; // protects buildingDatabase
  bool deleteMode;
  BuildingEventCallback onNewBuildingCallback;
  BuildingEventCallback onDeleteBuildingCallback;
  
  // NDEF parsing methods
  // Reads NDEF data into a dynamically sized buffer (heap). Returns true if any data was read.
  bool readNDEFData(std::vector<uint8_t>& ndefData);
  // Parses building type from NDEF data buffer. Returns 0 if not found.
  uint8_t parseNDEFBuildingType(const std::vector<uint8_t>& data);
  String getCardUID();
  
public:
  // Constructor
  NFCBuildingRegistry(MFRC522* reader);
  
  // Destructor
  ~NFCBuildingRegistry();
  
  // Main scanning method - call this in your loop()
  bool scanForCards();
  
  // Mode management
  void setDeleteMode(bool enabled);
  bool isDeleteMode() const;
  
  // Database management
  void clearDatabase();
  size_t getDatabaseSize() const;
  
  // Query methods
  std::map<String, BuildingCard> getAllBuildings() const; // snapshot copy under lock
  std::vector<BuildingCard> snapshotBuildings() const;    // lighter-weight snapshot
  std::map<String, BuildingCard*> getBuildingsByType(uint8_t buildingType);
  bool hasBuildingType(uint8_t buildingType) const;
  size_t getBuildingCount(uint8_t buildingType) const;
  
  // Card management
  bool addBuilding(const String& uid, uint8_t buildingType);
  bool removeBuilding(const String& uid);
  bool hasBuilding(const String& uid) const;
  BuildingCard* getBuilding(const String& uid);
  
  // Event callbacks
  void setOnNewBuildingCallback(BuildingEventCallback callback);
  void setOnDeleteBuildingCallback(BuildingEventCallback callback);
  
  // Utility methods
  void printDatabase() const;
  void printBuildingsByType(uint8_t buildingType) const;
  
  // Static utility methods
  static String uidToString(byte* uid, byte uidSize);
  static void printUID(byte* uid, byte uidSize);
};

#endif // NFC_BUILDING_REGISTRY_H
