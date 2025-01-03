# Bee Weight Scale

## Components

- ESP32 (+BLE)
- Load Cell x4
- HX711

## BLE Services and Characteristics
### General Service
- **UUID**: `d052cc98-92ab-420a-b19c-780f74650abc`
- **Characteristics**:
  - **Time Set**: `d052cc98-92ab-420a-b19c-780f74650ab1` (Write)

### Graph View Service
- **UUID**: `d052cc98-92ab-420a-b19c-780f74650aec`
- **Characteristics**:
  - **Date From**: `d052cc98-92ab-420a-b19c-780f74650ae1` (Read)
  - **Date To**: `d052cc98-92ab-420a-b19c-780f74650ae2` (Read)
  - **Number of Records**: `d052cc98-92ab-420a-b19c-780f74650ae6` (Read)
  - **Read Data**: `d052cc98-92ab-420a-b19c-780f74650ae5` (Notify)
  - **Delete Data**: `d052cc98-92ab-420a-b19c-780f74650ae7` (Write)
  - **Read Trigger**: `d052cc98-92ab-420a-b19c-780f74650ae8` (Write)

### Weight Service
- **UUID**: `d052cc98-92ab-420a-b19c-780f74650adc`
- **Characteristics**:
  - **Weight Read**: `d052cc98-92ab-420a-b19c-780f74650ad1` (Read, Notify)
  - **Weight Calibration**: `d052cc98-92ab-420a-b19c-780f74650ad2` (Write)
  - **Weight Tare Zero**: `d052cc98-92ab-420a-b19c-780f74650ad3` (Write)
