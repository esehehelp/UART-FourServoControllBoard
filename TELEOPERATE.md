# Teleoperate Command Implementation

## Overview

The **teleoperate (0x0A)** command enables synchronized servo control between reader and follower devices. This implementation allows independent control of multiple devices in a ring topology where:

- **Leader/Reader**: Sends position commands based on feedback from follower
- **Follower**: Responds with current servo feedback voltages
- **Synchronization**: Based on feedback ADC values from servo position sensors

## Command Specification

### Command Code: 0x0A (Teleoperate)

#### Request Packet Format
```
[HEADER] [TARGET] [SOURCE] [CMD] [LEN] [MODE] [CH_MASK] [CRC]
  0xAA    1-byte   1-byte   0x0A  0x02  1-byte  1-byte   1-byte
```

**Parameters:**
- **MODE** (1 byte):
  - `0x00` = Deactivate teleoperate mode
  - `0x01` = Activate teleoperate mode
  
- **CH_MASK** (1 byte):
  - Bitmask for channels: bit0=CH0, bit1=CH1, bit2=CH2, bit3=CH3
  - Example: `0x0F` activates all 4 channels
  - Example: `0x03` activates CH0 and CH1 only
  - Ignored when MODE=0x00

#### Response
No direct response to 0x0A command. However, when teleoperate mode is active:
- Device continues to respond to normal 0x02 (sensor read) commands
- Returns current feedback voltages in 0x82 response
- Accepts 0x01 (servo write) and 0x03 (sync write) commands as usual

## Usage Flow

### 1. Activate Teleoperate Mode (Reader)
```python
def activate_teleoperate(ser, channel_mask=0x0F, target=0x01):
    send_packet(ser, target, 0x0A, [0x01, channel_mask])
```

### 2. Position Calibration Phase
```python
# Send calibration sequence if needed using 0x07 command
send_packet(ser, target, 0x07, [channel, slope_bytes, intercept_bytes, ...])
```

### 3. Feedback Loop (Synchronization)
```python
def sync_servos_by_feedback(reader_port, follower_id):
    while teleoperate_active:
        # Read feedback from follower
        sensors = read_sensors(follower_id)
        follower_positions = sensors['feedback']
        
        # Compare and adjust reader positions
        for ch in range(4):
            if follower_positions[ch] != reader_positions[ch]:
                set_servo(reader_id, ch, follower_positions[ch])
```

### 4. Deactivate Teleoperate Mode
```python
def deactivate_teleoperate(ser, target=0x01):
    send_packet(ser, target, 0x0A, [0x00, 0x00])
```

## Implementation Details

### Firmware Changes

**Files Modified:**
1. `firmware/src/protocol.h`
   - Added command code constant: `#define CMD_TELEOPERATE 0x0A`
   - Added response code constants for clarity

2. `firmware/src/protocol.c`
   - Added teleoperate state struct:
     ```c
     typedef struct {
         uint8_t active;
         uint8_t channel_mask;
     } Teleoperate_t;
     ```
   - Implemented 0x0A handler in `Execute_Command()`:
     ```c
     case CMD_TELEOPERATE:
         if (len >= 2) {
             uint8_t mode = data[0];
             uint8_t ch_mask = data[1];
             if (mode == 0x00) {
                 g_teleoperate.active = 0;
                 g_teleoperate.channel_mask = 0;
             } else if (mode == 0x01) {
                 g_teleoperate.active = 1;
                 g_teleoperate.channel_mask = ch_mask & 0x0F;
             }
         }
         break;
     ```

### Key Features

1. **Simple State Machine**: Binary activate/deactivate with channel mask
2. **Runtime State**: Stored as volatile struct (not persisted to flash)
3. **Non-Intrusive**: Doesn't modify servo control or sensor reading
4. **Ring Topology**: Works seamlessly with existing device discovery and forwarding

## Testing

### Test Files

1. **test_teleoperate.py** - Dedicated teleoperate command test
   ```bash
   uv run firmware/test/test_teleoperate.py /dev/ttyACM0
   ```
   Tests:
   - Device discovery
   - Mode activation/deactivation
   - Feedback voltage reading while active
   - Servo position sweep with feedback tracking
   - Individual channel control
   - State transitions

2. **test_suite.py** - Comprehensive UART test suite including teleoperate
   ```bash
   uv run firmware/test/test_suite.py /dev/ttyACM0
   ```
   Tests all commands including:
   - LED control (0x05)
   - Sensor reading (0x02 → 0x82)
   - Servo control (0x01, 0x03)
   - Configuration (0x04, 0x08)
   - **Teleoperate mode (0x0A)** ← NEW

### Running Tests

```bash
cd firmware/test

# Run dedicated teleoperate test
uv run test_teleoperate.py /dev/ttyACM0

# Run full test suite
uv run test_suite.py /dev/ttyACM0

# Run existing tests (baseline verification)
uv run test_sensor.py /dev/ttyACM0
uv run test_ring.py /dev/ttyACM0
uv run test_led.py /dev/ttyACM0
```

## Protocol Integration

The teleoperate command integrates seamlessly with existing protocol:

### Command Precedence
- **During teleoperate mode**: Device still accepts all normal commands
- **0x01 (Write)**: Sets servo position
- **0x02 (Read)**: Returns current feedback
- **0x03 (SyncWrite)**: Sets all 4 servos
- **0x0A (Teleoperate)**: Activates/deactivates mode

### State Transitions
```
[Inactive] --0x0A(MODE=0x01)--> [Active]
[Active]   --0x0A(MODE=0x00)--> [Inactive]
```

### Feedback Path
```
Servo Move → Feedback ADC Sample → 0x02 Read → 0x82 Response
```

When teleoperate is active:
- Same feedback path as when inactive
- Enables leader to track follower position in real-time
- No latency penalty from teleoperate mode

## Multi-Device Scenarios

### Example: Dual-Device Master-Follower

**Setup:**
- Reader/Master: Device 0x01 (HOST role, USB connected)
- Follower/Slave: Device 0x02 (DEVICE role, ring-connected)

**Teleoperate Sequence:**

```python
# 1. Activate teleoperate on both devices
activate_teleoperate(master, 0x0F, target=0x01)
activate_teleoperate(master, 0x0F, target=0x02)  # Via ring routing

# 2. Master reads feedback from follower
while True:
    follower_fb = read_sensors(master, target=0x02)  # Routed via ring
    
    # 3. Master adjusts its position to match follower
    for ch in range(4):
        master_position = follower_fb['feedback'][ch]
        set_servo(master, ch, master_position, target=0x01)
    
    time.sleep(0.01)  # 100 Hz control loop

# 4. Deactivate when done
deactivate_teleoperate(master, target=0x01)
deactivate_teleoperate(master, target=0x02)
```

## Limitations & Notes

1. **Runtime Only**: Teleoperate state is not persisted. Device resets to inactive on power-on.
2. **No Hardware Changes**: Uses existing servo control and ADC feedback infrastructure.
3. **Simple State**: Binary on/off; could be extended for velocity control or impedance modes.
4. **Ring Routing**: Multi-device teleoperate relies on existing device discovery (0xA0/0xA1 ping/pong).
5. **CRC Protection**: All packets validated with CRC8; corrupted 0x0A commands are ignored.

## Future Extensions

Possible enhancements:

1. **Velocity Mode**: Add MODE=0x02 for velocity-based control
2. **Impedance Control**: Add MODE=0x03 for force/impedance feedback
3. **Persistence**: Store teleoperate preferences in flash config
4. **Multi-Follower**: Extend to 1-to-many synchronization (1 master → N followers)
5. **Filtering**: Add Kalman filtering to feedback stream for smoother tracking

## Build & Verification

### Firmware Build
```bash
cd firmware
pio run
# Output: .pio/build/genericCH32X035F7P6/firmware.bin
# Size check: RAM 21.6%, Flash 28.3% (with teleoperate)
```

### Flash to Device
```bash
pio run --target upload
```

### Verify Command Availability
```bash
# Check protocol.c for 0x0A handler
grep -n "CMD_TELEOPERATE" firmware/src/protocol.c
grep -n "case 0x0A" firmware/src/protocol.c
```

## References

- **Protocol Documentation**: See CLAUDE.md `Software Architecture` section
- **Test Framework**: `firmware/test/test_ring.py` (baseline UART test pattern)
- **ADC Feedback**: `firmware/src/adc.c` - `g_servo_feedback[]` array
- **Servo Control**: `firmware/src/servo.c` - `Set_Servo()` function
