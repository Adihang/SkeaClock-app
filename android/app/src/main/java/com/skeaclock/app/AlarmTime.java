package com.skeaclock.app;

import java.time.*;

final class AlarmTime {
  private AlarmTime() {}

  static long next(int hour, int minute, int days, long now, ZoneId zone) {
    LocalDate today = Instant.ofEpochMilli(now).atZone(zone).toLocalDate();
    for (int offset = 0; offset <= 7; offset++) {
      LocalDate date = today.plusDays(offset);
      if (days != 0 && (days & (1 << (date.getDayOfWeek().getValue() - 1))) == 0) continue;
      ZonedDateTime candidate = date.atTime(hour, minute).atZone(zone);
      if (candidate.toInstant().toEpochMilli() > now) return candidate.toInstant().toEpochMilli();
    }
    throw new IllegalStateException("No next alarm");
  }
}
