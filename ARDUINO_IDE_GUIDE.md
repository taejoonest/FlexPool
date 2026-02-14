# Arduino IDE File Organization Guide

## How Arduino IDE Organizes Files

### Basic Rules:

1. **Main Sketch File:**
   - Must be a `.ino` file
   - Must be in a folder with the **exact same name**
   - Example: `PumpController.ino` must be in `PumpController/` folder

2. **Additional Files:**
   - `.h` (header) files go in the same folder
   - `.cpp` (source) files go in the same folder
   - Arduino IDE automatically compiles all `.cpp` files

3. **File Structure Example:**

```
PumpController/              ← Folder name
├── PumpController.ino       ← Main file (must match folder name)
└── RS485Simple.h            ← Helper file (optional)
```

## Our Project Structure

### Controller ESP32
```
PumpController/
├── PumpController.ino       ← Main sketch (receives from USB, sends to RS-485)
└── RS485Simple.h            ← RS-485 library (helper class)
```

**Total: 2 files** (1 main .ino + 1 helper .h)

### Pump Simulator ESP32
```
PumpSimulator/
├── PumpSimulator.ino        ← Main sketch (receives from RS-485, responds)
├── RS485Simple.h            ← RS-485 library (helper class)
└── PumpSimulator.h          ← Pump simulation logic (helper class)
```

**Total: 3 files** (1 main .ino + 2 helper .h files)

## Opening in Arduino IDE

### Method 1: Open the .ino file
1. File → Open
2. Navigate to `PumpController/PumpController.ino`
3. Arduino IDE automatically loads all files in that folder

### Method 2: Open the folder
1. File → Open
2. Select the `PumpController` folder
3. Arduino IDE finds the `.ino` file automatically

## Why Not Just One File?

You **could** put everything in one `.ino` file, but:

**Single File (Messy):**
```cpp
// PumpController.ino - 500+ lines, hard to maintain
void setup() { ... }
void loop() { ... }
class RS485Simple { ... };  // Library code mixed in
// etc...
```

**Multiple Files (Clean):**
```cpp
// PumpController.ino - 50 lines, easy to read
#include "RS485Simple.h"
void setup() { ... }
void loop() { ... }

// RS485Simple.h - Separate, reusable library
class RS485Simple { ... };
```

## Best Practices

1. **Keep main `.ino` file simple** - just setup() and loop()
2. **Put reusable code in `.h` files** - like libraries
3. **Use descriptive names** - folder and .ino file must match
4. **One folder = one project** - each ESP32 gets its own folder

## Quick Reference

| What | Where | Example |
|------|-------|---------|
| Main code | `.ino` file | `PumpController.ino` |
| Helper classes | `.h` file | `RS485Simple.h` |
| Folder name | Must match .ino | `PumpController/` |
| All files | Same folder | All in `PumpController/` |
