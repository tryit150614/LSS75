#include "st_rtc.h"
#include "main.h"
#include "usart.h"
#include "nodeManager.h"
#include "flashWrapper.h"

extern struct MACStruct flashMAC;

Rtc rtc;

Rtc::Rtc() {

}

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
  if(flashRead(FLASH_FLAG_START_ADDR) != 0){

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
