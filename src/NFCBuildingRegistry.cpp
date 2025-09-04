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
  // Robust parser for a single-byte payload record with type 'B'
  if (data.empty()) return 0;

  auto within = [&](size_t idx) { return idx < data.size(); };

  size_t i = 0;
  while (i < data.size()) {
    uint8_t tlvType = data[i];

    if (tlvType == 0x00) { // NULL TLV (padding)
      i += 1;
      continue;
    }
    if (tlvType == 0xFE) { // Terminator TLV – stop
      break;
    }

    if (tlvType == 0x03) { // NDEF Message TLV
      // Read length (short or extended)
      if (!within(i + 1)) return 0; // malformed
      size_t lenFieldBytes = 1;
      size_t ndefLen = data[i + 1];
      if (ndefLen == 0xFF) { // extended length (2 bytes)
        if (!within(i + 3)) return 0; // malformed
        ndefLen = (static_cast<size_t>(data[i + 2]) << 8) | data[i + 3];
        lenFieldBytes = 3;
      }

      size_t msgStart = i + 1 + lenFieldBytes; // first byte of first record
      size_t msgEnd = msgStart + ndefLen;      // one past last byte of message
      if (msgStart >= data.size()) return 0;   // malformed
      if (msgEnd > data.size()) {              // Truncated read – clamp
        msgEnd = data.size();
      }

      size_t p = msgStart;
      while (p < msgEnd) {
        if (!within(p)) break;
        uint8_t header = data[p++];
        bool shortRecord = (header & 0x10) != 0; // SR bit
        bool hasId       = (header & 0x08) != 0; // IL bit
        uint8_t typeLen;
        size_t payloadLen = 0;

        if (!within(p)) break;
        typeLen = data[p++];

        if (shortRecord) {
          if (!within(p)) break;
          payloadLen = data[p++];
        } else {
          if (p + 4 > msgEnd) break; // not supporting large records fully
          payloadLen = (static_cast<size_t>(data[p]) << 24) |
                       (static_cast<size_t>(data[p + 1]) << 16) |
                       (static_cast<size_t>(data[p + 2]) << 8)  |
                       (static_cast<size_t>(data[p + 3]));
          p += 4;
        }

        uint8_t idLen = 0;
        if (hasId) {
          if (!within(p)) break;
            idLen = data[p++];
        }

        // Bounds for type / id / payload
        if (p + typeLen > msgEnd) break;
        const size_t typePos = p;
        p += typeLen;

        if (p + idLen > msgEnd) break;
        const size_t idPos = p;
        (void)idPos; // unused, but kept for clarity
        p += idLen;

        if (p + payloadLen > msgEnd) break; // truncated

        // Check for our custom record type 'B'
        if (typeLen == 1 && data[typePos] == 'B') {
          if (payloadLen >= 1) {
            return data[p]; // First (and only) byte is the building type
          } else {
            // Empty payload for 'B' – treat as not found
            return 0; 
          }
        }

        // Skip payload
        p += payloadLen;

        // If ME (Message End) bit set, stop parsing further records
        if (header & 0x40) break;
      }

      // Finished NDEF message without finding 'B'
      break; // Stop scanning TLVs
    }

    // Unsupported TLV – need to know its length to skip. TLVs with a length byte: 0x01,0x02,0x04.. etc.
    // For robustness, try to read a length; if impossible, abort.
    if (!within(i + 1)) break;
    uint8_t tlvLen = data[i + 1];
    if (tlvLen == 0xFF) { // extended format – need 2 more bytes
      if (!within(i + 3)) break;
      tlvLen = 0; // We don't actually need the true value for skipping safely here.
      i += 4;     // type + 0xFF + two length bytes (value skipped blindly)
    } else {
      i += 2 + tlvLen; // type + length + value
    }
  }

  // Fallback heuristic: raw short record pattern scan (flags, typeLen=1, payloadLen>=1, 'B', value)
  for (size_t k = 0; k + 4 < data.size(); ++k) {
    uint8_t flags = data[k];
    if ((flags & 0x10) == 0) continue; // Need SR
    uint8_t tLen = data[k + 1];
    uint8_t pLen = data[k + 2];
    if (tLen == 1 && pLen >= 1 && data[k + 3] == 'B') {
      return data[k + 4];
    }
  }

  return 0; // Not found
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
