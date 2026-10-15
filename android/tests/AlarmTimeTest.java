package com.skeaclock.app;

import java.time.*;

public class AlarmTimeTest {
  private static int checks;

  private static void check(
      String name, int h, int m, int days, String now, String expected, String zone) {
    ZoneId z = ZoneId.of(zone);
    long actual =
        AlarmTime.next(h, m, days, ZonedDateTime.parse(now).toInstant().toEpochMilli(), z);
    if (actual != ZonedDateTime.parse(expected).toInstant().toEpochMilli())
      throw new AssertionError(name + ": " + Instant.ofEpochMilli(actual));
    checks++;
  }

  public static void main(String[] args) {
    check(
        "one shot today",
        7,
        30,
        0,
        "2026-10-15T07:29:00+09:00",
        "2026-10-15T07:30:00+09:00",
        "Asia/Seoul");
    check(
        "exact boundary moves forward",
        7,
        30,
        0,
        "2026-10-15T07:30:00+09:00",
        "2026-10-16T07:30:00+09:00",
        "Asia/Seoul");
    check(
        "weekly rollover",
        7,
        30,
        8,
        "2026-10-15T07:31:00+09:00",
        "2026-10-22T07:30:00+09:00",
        "Asia/Seoul");
    check(
        "weekend skipped",
        7,
        30,
        31,
        "2026-10-16T08:00:00+09:00",
        "2026-10-19T07:30:00+09:00",
        "Asia/Seoul");
    check(
        "Sunday bit",
        9,
        0,
        64,
        "2026-10-17T12:00:00+09:00",
        "2026-10-18T09:00:00+09:00",
        "Asia/Seoul");
    check(
        "year rollover",
        0,
        0,
        127,
        "2026-12-31T23:59:59+09:00",
        "2027-01-01T00:00:00+09:00",
        "Asia/Seoul");
    check(
        "DST spring gap",
        2,
        30,
        0,
        "2026-03-08T00:00:00-05:00",
        "2026-03-08T03:30:00-04:00",
        "America/New_York");
    check(
        "DST overlap fires once",
        1,
        30,
        0,
        "2026-11-01T01:40:00-04:00",
        "2026-11-02T01:30:00-05:00",
        "America/New_York");
    System.out.println("PASS: " + checks + " scheduling cases");
  }
}
