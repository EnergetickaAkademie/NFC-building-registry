# NFC Building Registry Library

A comprehensive Arduino library for managing NFC building cards registry with support for multiple building types on ESP8266 and ESP32 platforms.

## Features

- 🏢 **Multi-building Support**: Register and manage different types of buildings (8-bit type system)
- 🔄 **Dual Mode Operation**: Normal registration mode and delete mode
- 📊 **Advanced Database**: Unordered map storage with efficient type-based queries
- 🎯 **Event Callbacks**: Customizable callbacks for new building detection and deletion
- 🔍 **NDEF Support**: Reads building types from NFC Forum Type 2 tags (NTAG213/215/216)
- 📱 **Cross-Platform**: Compatible with both ESP8266 and ESP32
- 🚀 **Easy Integration**: Simple API with comprehensive examples

## Supported Hardware

- **Microcontrollers**: ESP8266, ESP32
- **NFC Reader**: MFRC522 (via SPI)
- **NFC Tags**: NFC Forum Type 2 tags (NTAG213, NTAG215, NTAG216) and MIFARE Classic

## Installation

### PlatformIO (Recommended)

Add to your `platformio.ini`:

```ini
lib_deps = 
    git@github.com:EnergetickaAkademie/NFC-building-registry.git
```

### Arduino IDE

1. Download this repository as ZIP
2. Go to **Sketch** → **Include Library** → **Add .ZIP Library**
3. Select the downloaded ZIP file

## Wiring

### ESP8266 (Wemos D1 Mini)
| MFRC522 | Wemos D1 Mini |
|---------|---------------|
| SDA/SS  | D8 (GPIO 15)  |
| SCK     | D5 (GPIO 14)  |
| MOSI    | D7 (GPIO 13)  |
| MISO    | D6 (GPIO 12)  |
| IRQ     | Not connected |
| GND     | GND           |
| RST     | D3 (GPIO 0)   |
| 3.3V    | 3V3           |

### ESP32
| MFRC522 | ESP32    |
|---------|----------|
| SDA/SS  | GPIO 21  |
| SCK     | GPIO 18  |
| MOSI    | GPIO 23  |
| MISO    | GPIO 19  |
| IRQ     | Not connected |
| GND     | GND      |
| RST     | GPIO 22  |
| 3.3V    | 3V3      |

## Quick Start

```cpp
#include <NFCBuildingRegistry.h>

// Create MFRC522 instance
MFRC522 mfrc522(SS_PIN, RST_PIN);

// Create building registry
NFCBuildingRegistry buildingRegistry(&mfrc522);

void setup() {
  Serial.begin(115200);
  SPI.begin();
  mfrc522.PCD_Init();
  
  // Set up callback for new buildings
  buildingRegistry.setOnNewBuildingCallback([](uint8_t type, const String& uid) {
    Serial.println("New building: Type " + String(type) + ", UID: " + uid);
  });
}

void loop() {
  // Scan for cards and automatically manage the database
  buildingRegistry.scanForCards();
  delay(100);
}
```

## API Reference

### Core Methods

#### `bool scanForCards()`
Main scanning method. Call this in your `loop()` function. Returns `true` if a new building was added or removed.

#### `void setDeleteMode(bool enabled)`
Enable or disable delete mode. In delete mode, scanning a card removes it from the database instead of adding it.

#### `bool isDeleteMode() const`
Check if delete mode is currently enabled.

### Database Management

#### `void clearDatabase()`
Remove all buildings from the database.

#### `size_t getDatabaseSize() const`
Get the total number of registered buildings.

#### `std::map<String, BuildingCard> getAllBuildings() const`
Get all registered buildings as a map (UID → BuildingCard).

#### `std::map<String, BuildingCard*> getBuildingsByType(uint8_t buildingType)`
Get all buildings of a specific type.

#### `size_t getBuildingCount(uint8_t buildingType) const`
Count buildings of a specific type.

#### `bool hasBuildingType(uint8_t buildingType) const`
Check if any buildings of the specified type are registered.

### Card Management

#### `bool addBuilding(const String& uid, uint8_t buildingType)`
Manually add a building to the database.

#### `bool removeBuilding(const String& uid)`
Remove a building from the database by UID.

#### `bool hasBuilding(const String& uid) const`
Check if a building with the given UID is registered.

#### `BuildingCard* getBuilding(const String& uid)`
Get a pointer to a building record by UID.

### Event Callbacks

#### `void setOnNewBuildingCallback(BuildingEventCallback callback)`
Set callback function called when a new building is registered.

#### `void setOnDeleteBuildingCallback(BuildingEventCallback callback)`
Set callback function called when a building is deleted.

```cpp
// Callback signature
typedef std::function<void(uint8_t buildingType, const String& uid)> BuildingEventCallback;
```

### Utility Methods

#### `void printDatabase() const`
Print all registered buildings to Serial.

#### `void printBuildingsByType(uint8_t buildingType) const`
Print buildings of a specific type to Serial.

#### `static String uidToString(byte* uid, byte uidSize)`
Convert UID bytes to hex string.

## Building Types

The library uses an 8-bit building type system (0-255). You can define your own building types:

```cpp
#define BUILDING_TYPE_HOUSE       1
#define BUILDING_TYPE_APARTMENT   2
#define BUILDING_TYPE_OFFICE      3
#define BUILDING_TYPE_SHOP        4
#define BUILDING_TYPE_FACTORY     5
#define BUILDING_TYPE_WAREHOUSE   6
#define BUILDING_TYPE_SCHOOL      7
#define BUILDING_TYPE_HOSPITAL    8
```

## BuildingCard Structure

```cpp
struct BuildingCard {
  String uid;              // Card UID as hex string
  uint8_t buildingType;    // Building type (8-bit)
  unsigned long firstSeen; // Timestamp when first registered
  unsigned long lastSeen;  // Timestamp when last seen
};
```

## NDEF Support

The library can read building types from NDEF records on NFC Forum Type 2 tags. To write building type data to a tag, use an NDEF record with:
- **Type**: "B" (single character)
- **Payload**: Single byte containing the building type (0-255)

If NDEF reading fails, the library falls back to using the first byte of the card UID as the building type.

## Examples

### Basic Usage
Simple card registration with callbacks.

### Advanced Game
Interactive game where players collect specific building types with statistics and progress tracking.

## Error Handling

The library includes comprehensive error handling:
- Null pointer checks for MFRC522 instance
- NDEF parsing failure fallbacks
- Database consistency checks
- Invalid parameter validation

## Memory Usage

- **RAM**: Approximately 50-100 bytes per registered building
- **Flash**: ~15KB library code
- **Database**: Uses STL map for efficient storage and lookup

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests if applicable
5. Submit a pull request

## License

This library is released under the MIT License. See LICENSE file for details.

## Support

- **Issues**: Report bugs and request features on GitHub
- **Documentation**: Check the examples and API reference
- **Community**: Join discussions in the Issues section

## Changelog

### v1.0.0
- Initial release
- Core functionality for building registration
- NDEF support for NFC Forum Type 2 tags
- ESP8266 and ESP32 compatibility
- Comprehensive examples and documentation
