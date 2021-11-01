#include "st_rtc.h"
#include "main.h"
#include "usart.h"

Rtc rtc;
Rtc::Rtc() {

}

uint32_t month2day(uint32_t month);

void Rtc::init() {
  LL_RTC_InitTypeDef RTC_InitStruct = {0};

  LL_PWR_EnableBkUpAccess();

  /**Initialize RTC and set the Time and Date
  */
  RTC_InitStruct.HourFormat = LL_RTC_HOURFORMAT_24HOUR;
  RTC_InitStruct.AsynchPrescaler = 127;
  RTC_InitStruct.SynchPrescaler = 255;

  LL_RTC_Init(RTC, &RTC_InitStruct);
  LL_RTC_SetAsynchPrescaler(RTC, 127);
  LL_RTC_SetSynchPrescaler(RTC, 255);
  /**Initialize RTC and set the Time and Date
  */
  if(LL_RTC_BAK_GetRegister(RTC, LL_RTC_BKP_DR1) != 100){
    LL_RTC_BAK_SetRegister(RTC, LL_RTC_BKP_DR1, 100);

    Rtc::setTime(0, 0, 0);
    Rtc::setDate(21, 7, 14);

    writeString(DEBUG_USART, "Init RTC time.\r\n");
  }

  LL_PWR_DisableBkUpAccess();
}

void Rtc::setDate(std::uint8_t year, std::uint8_t month, std::uint8_t day) {
  LL_PWR_EnableBkUpAccess();
  LL_RTC_DateTypeDef RTC_DateStruct = {0};
  RTC_DateStruct.Day = day;
  RTC_DateStruct.Month = month;
  RTC_DateStruct.Year = year;
  LL_RTC_DATE_Init(RTC, LL_RTC_FORMAT_BIN, &RTC_DateStruct);
  LL_PWR_DisableBkUpAccess();
}

void Rtc::setTime(std::uint8_t hour, std::uint8_t minute, std::uint8_t second) {
  LL_PWR_EnableBkUpAccess();
  LL_RTC_TimeTypeDef RTC_TimeStruct = {0};
  RTC_TimeStruct.Hours = hour;
  RTC_TimeStruct.Minutes = minute;
  RTC_TimeStruct.Seconds = second;
  LL_RTC_TIME_Init(RTC, LL_RTC_FORMAT_BIN, &RTC_TimeStruct);
  LL_PWR_DisableBkUpAccess();
}

std::uint32_t Rtc::getYear(void) {
  return __LL_RTC_CONVERT_BCD2BIN(LL_RTC_DATE_GetYear(RTC));
}

std::uint32_t Rtc::getMonth(void) {
  return __LL_RTC_CONVERT_BCD2BIN(LL_RTC_DATE_GetMonth(RTC));
}

std::uint32_t Rtc::getDay(void) {
  return __LL_RTC_CONVERT_BCD2BIN(LL_RTC_DATE_GetDay(RTC));
}

std::uint32_t Rtc::getHour(void) {
  return __LL_RTC_CONVERT_BCD2BIN(LL_RTC_TIME_GetHour(RTC));
}

std::uint32_t Rtc::getMinute(void) {
  return __LL_RTC_CONVERT_BCD2BIN(LL_RTC_TIME_GetMinute(RTC));
}

std::uint32_t Rtc::getSecond(void) {
  return __LL_RTC_CONVERT_BCD2BIN(LL_RTC_TIME_GetSecond(RTC));
}

void RTC_SetAlert(uint32_t t) {
  LL_PWR_EnableBkUpAccess();
  LL_RTC_DisableWriteProtection(RTC);
  LL_RTC_ClearFlag_WUT(RTC);
  __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);
  RTC->CR &= ~RTC_CR_WUTE;
  HAL_Delay(1);
  RTC->WUTR = t;
  RTC->CR = RTC_CR_WUTE | RTC_CR_WUTIE | RTC_CR_WUCKSEL_2;
  LL_RTC_EnableWriteProtection(RTC);
  EXTI->RTSR1 |= EXTI_RTSR1_RT20;
  EXTI->IMR1 |= EXTI_IMR1_IM20;
  NVIC_EnableIRQ(RTC_WKUP_IRQn);
  NVIC_SetPriority(RTC_WKUP_IRQn, 2);
  LL_PWR_DisableBkUpAccess();
}

uint32_t getUnixTimestamp() {

  uint8_t leapYearCount = 0;
  uint32_t baseSeconds = 946684800; // It's 1970/1/1~2020/1/1 total seconds

  volatile uint32_t second = rtc.getSecond();
  volatile uint32_t minute = rtc.getMinute();
  volatile uint32_t hour = rtc.getHour();
  volatile uint32_t day = rtc.getDay();
  volatile uint32_t month = rtc.getMonth();
  volatile uint32_t year = rtc.getYear();

  leapYearCount = year / 4 + 1; //unit : day
  if(year % 4 == 0 && month < 3) leapYearCount--;

  return  baseSeconds + (year * 365 * 86400) +
          (month2day(month) + day + leapYearCount - 1) * 86400 +
          (hour * 3600) + (minute * 60) + second;
}

uint32_t month2day(uint32_t month) {
  switch(month - 1) {
    case 0: return 0;
    break;
    case 1: return 31;
    break;
    case 2: return 31+28;
    break;
    case 3: return 31+28+31;
    break;
    case 4: return 31+28+31+30;
    break;
    case 5: return 31+28+31+30+31;
    break;
    case 6: return 31+28+31+30+31+30;
    break;
    case 7: return 31+28+31+30+31+30+31;
    break;
    case 8: return 31+28+31+30+31+30+31+31;
    break;
    case 9: return 31+28+31+30+31+30+31+31+30;
    break;
    case 10: return 31+28+31+30+31+30+31+31+30+31;
    break;
    case 11: return 31+28+31+30+31+30+31+31+30+31+30;
    break;
  }

  return 0;
}