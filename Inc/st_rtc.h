#ifndef _RTC_H
#define _RTC_H

#include <cstdint>

class Rtc {
public:
  Rtc();

  void init(void);
  void setDate(std::uint8_t year, std::uint8_t month, std::uint8_t day);
  void setTime(std::uint8_t hour, std::uint8_t minute, std::uint8_t second);
  std::uint32_t getYear(void);
  std::uint32_t getMonth(void);
  std::uint32_t getDay(void);
  std::uint32_t getHour(void);
  std::uint32_t getMinute(void);
  std::uint32_t getSecond(void);
private:

};

#endif /* _RTC_H */
