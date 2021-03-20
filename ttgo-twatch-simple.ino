#include "config.h"
#include <WiFi.h>

#define CPU_FREQ_MIN 10
#define CPU_FREQ_NORM 80
#define CPU_FREQ_WIFI 80
#define CPU_FREQ_MEDIUM 160
#define CPU_FREQ_MAX 240

#define COLOR_ORANGE 0xFBE0
#define COLOR_GREY 0x39C4

#define DEFAULT_SCREEN_TIMEOUT 5000

TTGOClass *ttgo;
TFT_eSPI *tft ;
AXP20X_Class *power;

bool irq = false;
bool powerOff = false;
byte xcolon = 0;
uint32_t lastOnTime = 0;

void displayTimeAndBattery()
{
  int battery_percentage = power->getBattPercentage();

  tft->setTextSize(2);
  tft->setTextColor(COLOR_ORANGE, TFT_BLACK);
  tft->setCursor(10, 10);

  if (battery_percentage < 10)
  {
    tft->print("  ");
  }
  else if (battery_percentage < 100)
  {
    tft->print(" ");
  }

  tft->print(battery_percentage);
  tft->println("%");

  tft->setTextSize(1);

  RTC_Date dateTimeNow = ttgo->rtc->getDateTime();

  uint8_t hh = dateTimeNow.hour;
  uint8_t mm = dateTimeNow.minute;
  uint8_t ss = dateTimeNow.second;
  uint8_t dday = dateTimeNow.day;
  uint8_t mmonth = dateTimeNow.month;
  uint16_t yyear = dateTimeNow.year;

  byte xpos = 40;
  byte ypos = 90;

  if (hh < 10)
  {
    xpos += ttgo->tft->drawChar('0', xpos, ypos, 7);
  }

  xpos += ttgo->tft->drawNumber(hh, xpos, ypos, 7);
  xcolon = xpos + 3;
  xpos += ttgo->tft->drawChar(':', xcolon, ypos, 7);

  if (mm < 10)
  {
    xpos += ttgo->tft->drawChar('0', xpos, ypos, 7);
  }

  tft->drawNumber(mm, xpos, ypos, 7);

  if (ss % 2)
  {
    tft->setTextColor(COLOR_GREY, TFT_BLACK);
    xpos += tft->drawChar(':', xcolon, ypos, 7);
    tft->setTextColor(COLOR_ORANGE, TFT_BLACK);
  }
  else
  {
    tft->drawChar(':', xcolon, ypos, 7);
  }

  tft->setTextSize(3);
  tft->setCursor(10, 210);
  tft->print(dday);
  tft->print(".");
  tft->print(mmonth);
  tft->print(".");
  tft->print(yyear);
}

void enterSleepMode()
{
  ttgo->closeBL();
  ttgo->displaySleep();

  if (!WiFi.isConnected())
  {
    delay(250);
    WiFi.mode(WIFI_OFF);

    setCpuFrequencyMhz(CPU_FREQ_MIN);

    gpio_wakeup_enable((gpio_num_t)AXP202_INT, GPIO_INTR_LOW_LEVEL);

    esp_sleep_enable_gpio_wakeup();
    esp_light_sleep_start();
  }
}

void enterDeepSleepMode()
{
  tft->setTextSize(2);
  tft->setTextColor(COLOR_ORANGE, TFT_BLACK);
  tft->setCursor(10, 10);
  tft->print("Going to sleep...");

  delay(2000);

  ttgo->powerOff();

  esp_sleep_enable_ext1_wakeup(GPIO_SEL_35, ESP_EXT1_WAKEUP_ALL_LOW);
  esp_deep_sleep_start();
}

void wakeUpFromSleepMode()
{
  setCpuFrequencyMhz(CPU_FREQ_NORM);

  ttgo->displayWakeup();
  ttgo->rtc->syncToSystem();

  ttgo->openBL();

  lastOnTime = millis();
}

void setup(void)
{
  Serial.begin(115200);

  ttgo = TTGOClass::getWatch();
  ttgo->begin();
  ttgo->openBL();
  ttgo->setBrightness(85);

  tft = ttgo->tft;
  power = ttgo->power;

  pinMode(AXP202_INT, INPUT_PULLUP);
  attachInterrupt(AXP202_INT, [] {
    irq = true;
  }, FALLING);

  power->adc1Enable(AXP202_BATT_VOL_ADC1 | AXP202_BATT_CUR_ADC1 | AXP202_VBUS_VOL_ADC1 | AXP202_VBUS_CUR_ADC1, AXP202_ON);
  power->enableIRQ(AXP202_PEK_SHORTPRESS_IRQ | AXP202_PEK_LONGPRESS_IRQ, true);
  power->clearIRQ();
}

void loop(void)
{
  if (irq)
  {
    irq = false;

    power->readIRQ();

    if (power->isPEKShortPressIRQ() )
    {
      if (ttgo->bl->isOn())
      {
        enterSleepMode();
      }
      else
      {
        wakeUpFromSleepMode();
      }
    }
    else if (power->isPEKLongtPressIRQ() )
    {
      powerOff = true;
    }

    power->clearIRQ();
  }

  if (powerOff)
  {
    enterDeepSleepMode();
  }

  displayTimeAndBattery();

  uint32_t currentMillis = millis();
  if (currentMillis > (lastOnTime + DEFAULT_SCREEN_TIMEOUT))
  {
    enterSleepMode();
  }

  delay(1000);
}
