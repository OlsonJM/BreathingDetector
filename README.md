# BreathingDetector
Breathing Monitor using Melexis MLX90614-DAA IR Sensor

Detect delta in temperature on pediatric aerosol nose/mouth face mask.

Utilizing the Adafruit Feather M0 Adalogger.

   Hardware:
      (1) Adafruit Feather M0 Adalogger: https://www.adafruit.com/product/2796
      (1) Adafruit Feather OLED display: https://www.adafruit.com/product/2900
      (1) Adafruit Feather Backplane (doubler): https://www.adafruit.com/product/2890
      (1) Melexis MLX90614 IR Sensor:https://www.adafruit.com/product/1747
          Sensor Part#: MLX90614ESF-DAA (3Vdc Medical Accuracy version, using I2C bus)
          Spec Sheet: https://www.melexis.com/-/media/files/documents/datasheets/mlx90614-datasheet-melexis.pdf
      (1) 5mm RED LED       (ALARM)
      (1) 5mm GREEN LED     (EXHALE)
      (1) 5mm WHITE LED     (INHALE)
      (1) 5mm BLUE LED      (PATIENT PRESENT)
      (4) 1k, 1/4W resistor (for LED's) (R1, R2, R3, R4)
      (1) 0.1uF Ceramic Capacitor (Vdd-->Vss at MLX sensor) (C1)
      
Make the following wire connections if using the hardware & pin #'s in the base code:

M0 PIN SCL --> MLX PIN SCL (Clock) (SEE NOTE1)
M0 PIN SDA --> MLX PIN SDA (Data)  (SEE NOTE1)
M0 PIN 3V  --> MLX PIN Vdd
M0 PIN GND --> MLX PIN Vss (GND)
CAP C1     --> ACCROSS Vdd & Vss of MLX sensor
M0 PIN GND --> LED Cathode (short leg) White, Red, Blue, & Green
M0 PIN 13  --> R1 --> RED LED Annode   (SEE NOTE2)
M0 PIN 12  --> R2 --> GREED LED Annode
M0 PIN 11  --> R3 --> WHITE LED Annode
M0 PIN 10  --> R4 --> BLUDE LED Annode

NOTE1: PULL-UP RESISTORS NOT REQUIRED ON I2C BUS IF USING OLED DISPLAY. ADD 10K PULLUPS BETWEEN 3V BUS AND SDA & SCL IF DISPLAY IS NOT USED.

NOTE2: PIN 13 ON FEATHER M0 IS ALSO CONNECTED TO THE ONBOARD LED. AS SUCH, THE RED LED WILL ALSO FLASH WHEN UPLOADING CODE TO BOARD.
