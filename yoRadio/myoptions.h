#ifndef myoptions_h
#define myoptions_h

/*
  yoRadio configuration for:
  ESP32-WROOM / HW-724
  Built-in 128x64 SSD1306 OLED
  Internal ESP32 DAC
  HW-724 Rotary Encoder
*/

#define L10N_LANGUAGE EN

/* =====================================================
   BUILT-IN OLED
   HW-724 uses GPIO5 = SDA
   HW-724 uses GPIO4 = SCL
   OLED address = 0x3C
   ===================================================== */

#define DSP_MODEL DSP_SSD1306

#define I2C_SDA 5
#define I2C_SCL 4


/* =====================================================
   HW-724 ROTARY ENCODER

   A / CLK = GPIO16
   B / DT  = GPIO14
   SW      = GPIO13
   ===================================================== */

#define ENC_BTNL 13
#define ENC_BTNB 16
#define ENC_BTNR 14
#define ENC_INTERNALPULLUP true


/* =====================================================
   INTERNAL ESP32 DAC
   ===================================================== */

#define I2S_INTERNAL true

#endif
