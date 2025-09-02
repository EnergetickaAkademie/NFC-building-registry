#include "NFCBuildingRegistry.h"

NFCBuildingRegistry::NFCBuildingRegistry(MFRC522* reader) 
  : mfrc522(reader), deleteMode(false) {
  if (!mfrc522) {
    Serial.println("ERROR: MFRC522 reader is null!");
  }
}

NFCBuildingRegistry::~NFCBuildingRegistry() {
  // Cleanup if needed
}

bool NFCBuildingRegistry::scanForCards() {
  if (!mfrc522) {
    return false;
  }
  
  // Look for new cards
  if (!mfrc522->PICC_IsNewCardPresent()) {
    return false;
  }
  
  // Select one of the cards
  if (!mfrc522->PICC_ReadCardSerial()) {
    return false;
  }
  
  // Get card UID
  String uid = getCardUID();
  
  // Try to read building type from NDEF data
  std::vector<uint8_t> ndefData;
  uint8_t buildingType = 0;
  
  if (readNDEFData(ndefData)) {
    buildingType = parseNDEFBuildingType(ndefData);
    if (buildingType == 0) {
      // Distinguish between true type 0 and parse failure by checking for 'B' marker manually
      bool hasMarkerB = false;
      for (size_t i = 0; i + 4 < ndefData.size(); ++i) {
        if (ndefData[i] == 'B') { hasMarkerB = true; break; }
      }
      if (!hasMarkerB) {
        Serial.println("NDEF parsed but no building record found; defaulting to 0.");
      }
    }
  } else {
    // If NDEF reading fails, try to use the first byte of UID as building type
    // This is a fallback method - you might want to implement a different strategy
    if (mfrc522->uid.size > 0) {
      buildingType = mfrc522->uid.uidByte[0] % 256; // Ensure it's 8-bit
    }
  }
  
  // Halt the card
  mfrc522->PICC_HaltA();
  
  // Process the card based on current mode
  if (deleteMode) {
    // Delete mode: remove building if present
      if (hasBuilding(uid)) {
      uint8_t removedBuildingType = 0;
      {
        std::lock_guard<std::mutex> lock(dbMutex);
        removedBuildingType = buildingDatabase.find(uid)->second.buildingType; 
      }
      bool removed = removeBuilding(uid);
      
      if (removed) {
        // Call callback after successful removal
        if (onDeleteBuildingCallback) {
          onDeleteBuildingCallback(removedBuildingType, uid);
        }
        Serial.println("Building removed: UID=" + uid + ", Type=" + String(removedBuildingType));
        return true;
      }
    } else {
      // Building not found in delete mode - this is normal for multiple scans
      Serial.println("Building not found for deletion: UID=" + uid);
    }
  } else {
    // Add mode: add building if not present
    if (!hasBuilding(uid)) {
      addBuilding(uid, buildingType);
      if (onNewBuildingCallback) {
        onNewBuildingCallback(buildingType, uid);
      }
      Serial.println("New building added: UID=" + uid + ", Type=" + String(buildingType));
      return true;
    } else {
      // Update last seen timestamp
      {
        std::lock_guard<std::mutex> lock(dbMutex);
        auto it = buildingDatabase.find(uid);
        if (it != buildingDatabase.end()) it->second.lastSeen = millis();
      }
      Serial.println("Building already registered: UID=" + uid);
    }
  }
  
  return false;
}

void NFCBuildingRegistry::setDeleteMode(bool enabled) {
  deleteMode = enabled;
  Serial.println("Delete mode: " + String(enabled ? "ENABLED" : "DISABLED"));
}

bool NFCBuildingRegistry::isDeleteMode() const {
  return deleteMode;
}

void NFCBuildingRegistry::clearDatabase() {
  std::lock_guard<std::mutex> lock(dbMutex);
  buildingDatabase.clear();
  Serial.println("Building database cleared");
}

size_t NFCBuildingRegistry::getDatabaseSize() const {
  std::lock_guard<std::mutex> lock(dbMutex);
  return buildingDatabase.size();
}

std::map<String, BuildingCard> NFCBuildingRegistry::getAllBuildings() const {
  std::lock_guard<std::mutex> lock(dbMutex);
  return buildingDatabase; // copy
}

std::vector<BuildingCard> NFCBuildingRegistry::snapshotBuildings() const {
  std::vector<BuildingCard> out;
  std::lock_guard<std::mutex> lock(dbMutex);
  out.reserve(buildingDatabase.size());
  for (const auto &p : buildingDatabase) out.push_back(p.second);
  return out;
}

std::map<String, BuildingCard*> NFCBuildingRegistry::getBuildingsByType(uint8_t buildingType) {
  std::map<String, BuildingCard*> result;
  std::lock_guard<std::mutex> lock(dbMutex);
  for (auto& pair : buildingDatabase) {
    if (pair.second.buildingType == buildingType) {
      result[pair.first] = &pair.second;
    }
  }
  return result;
}

bool NFCBuildingRegistry::hasBuildingType(uint8_t buildingType) const {
  std::lock_guard<std::mutex> lock(dbMutex);
  for (const auto& pair : buildingDatabase) {
    if (pair.second.buildingType == buildingType) {
      return true;
    }
  }
  return false;
}

size_t NFCBuildingRegistry::getBuildingCount(uint8_t buildingType) const {
  size_t count = 0;
  std::lock_guard<std::mutex> lock(dbMutex);
  for (const auto& pair : buildingDatabase) {
    if (pair.second.buildingType == buildingType) {
      count++;
    }
  }
  return count;
}

bool NFCBuildingRegistry::addBuilding(const String& uid, uint8_t buildingType) {
  if (uid.length() == 0) {
    return false;
  }
  
  std::lock_guard<std::mutex> lock(dbMutex);
  if (buildingDatabase.find(uid) != buildingDatabase.end()) {
    // Building already exists, update last seen
    buildingDatabase[uid].lastSeen = millis();
    return false;
  }
  
  // Add new building
  BuildingCard newCard(uid, buildingType);
  buildingDatabase[uid] = newCard;
  
  return true;
}

bool NFCBuildingRegistry::removeBuilding(const String& uid) {
  std::lock_guard<std::mutex> lock(dbMutex);
  return buildingDatabase.erase(uid) > 0;   // returns # of nodes erased
}


bool NFCBuildingRegistry::hasBuilding(const String& uid) const {
  std::lock_guard<std::mutex> lock(dbMutex);
  return buildingDatabase.find(uid) != buildingDatabase.end();
}

BuildingCard* NFCBuildingRegistry::getBuilding(const String& uid) {
  std::lock_guard<std::mutex> lock(dbMutex);
  auto it = buildingDatabase.find(uid);
  if (it != buildingDatabase.end()) {
    return &it->second;
  }
  return nullptr;
}

void NFCBuildingRegistry::setOnNewBuildingCallback(BuildingEventCallback callback) {
  onNewBuildingCallback = callback;
}

void NFCBuildingRegistry::setOnDeleteBuildingCallback(BuildingEventCallback callback) {
  onDeleteBuildingCallback = callback;
}

void NFCBuildingRegistry::printDatabase() const {
  std::lock_guard<std::mutex> lock(dbMutex);
  Serial.println("=== Building Database ===");
  Serial.println("Total buildings: " + String(buildingDatabase.size()));
  
  for (const auto& pair : buildingDatabase) {
    const BuildingCard& card = pair.second;
    Serial.println("UID: " + card.uid + 
                   " | Type: " + String(card.buildingType) + 
                   " | First: " + String(card.firstSeen) + 
                   " | Last: " + String(card.lastSeen));
  }
  Serial.println("========================");
}

void NFCBuildingRegistry::printBuildingsByType(uint8_t buildingType) const {
  std::lock_guard<std::mutex> lock(dbMutex);
  Serial.println("=== Buildings of Type " + String(buildingType) + " ===");
  
  int count = 0;
  for (const auto& pair : buildingDatabase) {
    if (pair.second.buildingType == buildingType) {
      const BuildingCard& card = pair.second;
      Serial.println("UID: " + card.uid + 
                     " | First: " + String(card.firstSeen) + 
                     " | Last: " + String(card.lastSeen));
      count++;
    }
  }
  
  Serial.println("Total: " + String(count) + " buildings");
  Serial.println("============================");
}

String NFCBuildingRegistry::getCardUID() {
  if (!mfrc522) {
    return "";
  }
  
  return uidToString(mfrc522->uid.uidByte, mfrc522->uid.size);
}

bool NFCBuildingRegistry::readNDEFData(std::vector<uint8_t>& ndefData) {
  if (!mfrc522) {
    return false;
  }
  
  // Check if it's an NTAG or similar NFC Forum Type 2 tag
  MFRC522::PICC_Type piccType = mfrc522->PICC_GetType(mfrc522->uid.sak);
  if (piccType != MFRC522::PICC_TYPE_MIFARE_UL) {
    return false; // Not an NFC Forum Type 2 tag
  }
  
  // Working buffer for MFRC522::MIFARE_Read (16 bytes of data + 2 CRC bytes)
  std::vector<byte> buffer(18);
  byte size = static_cast<byte>(buffer.size());
  MFRC522::StatusCode status;
  
  // Read capability container (CC) at page 3
  status = mfrc522->MIFARE_Read(3, buffer.data(), &size);
  if (status != MFRC522::STATUS_OK) {
    return false;
  }
  
  // Check if NDEF is present (magic number 0xE1)
  if (buffer[0] != 0xE1) {
    return false;
  }
  
  // Read NDEF data starting from page 4
  ndefData.clear();
  ndefData.reserve(64); // common small NDEF sizes; auto-expands if needed
  
  // Read multiple pages to get the full NDEF message
  for (int page = 4; page < 20; page += 4) {
    size = static_cast<byte>(buffer.size());
    status = mfrc522->MIFARE_Read(page, buffer.data(), &size);
    if (status != MFRC522::STATUS_OK) {
      break;
    }
    
    // Copy 16 bytes of data to NDEF buffer (skip last 2 CRC bytes)
    for (int i = 0; i < 16; i++) {
      ndefData.push_back(buffer[i]);
    }
    
    // If we encounter terminator TLV (0xFE), stop reading
    for (int i = 0; i < 16; i++) {
      if (buffer[i] == 0xFE) {
        return !ndefData.empty();
      }
    }
  }
  
  return !ndefData.empty();
}

uint8_t NFCBuildingRegistry::parseNDEFBuildingType(const std::vector<uint8_t>& data) {
  const int length = static_cast<int>(data.size());
  if (length < 3) {
    return 0;
  }

  int pos = 0;

  // Look for NDEF TLV (Type-Length-Value)
  while (pos < length - 1) {
    if (data[pos] == 0x03) {  // NDEF Message TLV
      pos++;  // Move past the type byte

      // Get length (only supports short length < 0xFF for simplicity)
      int ndefLength = data[pos];
      pos++;

      if (ndefLength <= 0 || pos + ndefLength > length) {
        return 0;
      }

      // Parse NDEF records - look for custom building type record
      int recordPos = 0;
      while (recordPos < ndefLength) {
        if (pos + recordPos >= length) break;

        byte flags = data[pos + recordPos];
        recordPos++;

        bool isShort = (flags & 0x10) != 0;  // SR bit
        byte typeLength = 0;
        uint32_t payloadLength = 0;

        // Type Length
        if (pos + recordPos >= length) break;
        typeLength = data[pos + recordPos];
        recordPos++;

        // Payload Length
        if (isShort) {
          if (pos + recordPos >= length) break;
          payloadLength = data[pos + recordPos];
          recordPos++;
        } else {
          // 4-byte length (shouldn't happen for our use case)
          if (recordPos + 4 > ndefLength) break;
          payloadLength = (uint32_t)data[pos + recordPos] << 24 |
                          (uint32_t)data[pos + recordPos + 1] << 16 |
                          (uint32_t)data[pos + recordPos + 2] << 8  |
                          (uint32_t)data[pos + recordPos + 3];
          recordPos += 4;
        }

        // ID Length if IL flag set
        uint8_t idLength = 0;
        if (flags & 0x08) { // IL flag
          if (pos + recordPos >= length) break;
          idLength = data[pos + recordPos];
          recordPos++;
        }

        // Type field
        if (recordPos + typeLength > ndefLength) break;
        String recordType = "";
        for (int i = 0; i < typeLength; i++) {
          recordType += (char)data[pos + recordPos + i];
        }
        recordPos += typeLength;

        // Skip ID field if present
        if (idLength) {
          if (recordPos + idLength > ndefLength) break;
          recordPos += idLength;
        }

        // Payload - check if this is our building record
        if (recordPos + (int)payloadLength > ndefLength) break;
        if (recordType == "B" && payloadLength >= 1) {
          return data[pos + recordPos];
        }
        recordPos += payloadLength;

        // If ME (Message End) bit set, stop parsing further records
        if (flags & 0x40) break;
      }

      return 0;
    }
    pos++;
  }

  // Fallback heuristic: scan raw bytes for a short record with type 'B'
  // This handles cases where TLV parsing fails but the data is still there
  for (int i = 0; i < length - 5; i++) {
    byte flags = data[i];
    if ((flags & 0x10) == 0) continue; // need SR bit
    byte typeLen = data[i + 1];
    if (typeLen != 1) continue;
    byte payloadLen = data[i + 2];
    if (payloadLen < 1) continue;
    if (data[i + 3] == 'B') { // type byte
      return data[i + 4]; // payload byte
    }
  }

  return 0; // No building type found
}

String NFCBuildingRegistry::uidToString(byte* uid, byte uidSize) {
  String result = "";
  for (byte i = 0; i < uidSize; i++) {
    if (uid[i] < 0x10) {
      result += "0";
    }
    result += String(uid[i], HEX);
  }
  result.toUpperCase();
  return result;
}

void NFCBuildingRegistry::printUID(byte* uid, byte uidSize) {
  Serial.print("UID: ");
  for (byte i = 0; i < uidSize; i++) {
    if (uid[i] < 0x10) {
      Serial.print("0");
    }
    Serial.print(uid[i], HEX);
    if (i < uidSize - 1) {
      Serial.print(" ");
    }
  }
  Serial.println();
}
