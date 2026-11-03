package com.skeaclock.app;

import java.time.*;
import android.content.Context;
import org.json.JSONObject;

final class Alarm {
  int id, hour, minute, days;
  boolean enabled;
  String label;

  Alarm(int id, int hour, int minute, int days, boolean enabled, String label) {
    this.id = id;
    this.hour = hour;
    this.minute = minute;
    this.days = days;
    this.enabled = enabled;
    this.label = label;
  }

  static Alarm from(JSONObject o) {
    return new Alarm(
        o.optInt("id"),
        o.optInt("hour", 7),
        o.optInt("minute"),
        o.optInt("days"),
        o.optBoolean("enabled"),
        o.optString("label", "알람"));
  }

  JSONObject json() {
    JSONObject o = new JSONObject();
    try {
      o.put("id", id)
          .put("hour", hour)
          .put("minute", minute)
          .put("days", days)
          .put("enabled", enabled)
          .put("label", label);
    } catch (org.json.JSONException e) {
      throw new IllegalStateException(e);
    }
    return o;
  }

  long next(long now, ZoneId zone) {
    return AlarmTime.next(hour, minute, days, now, zone);
  }

  String time() {
    return String.format(java.util.Locale.KOREA, "%02d:%02d", hour, minute);
  }

  String repeat(Context context) {
    if (days == 0) return context.getString(R.string.repeat_once);
    if (days == 127) return context.getString(R.string.repeat_daily);
    StringBuilder s = new StringBuilder();
    String[] names = context.getResources().getStringArray(R.array.weekday_names);
    for (int i = 0; i < 7; i++) if ((days & (1 << i)) != 0) s.append(names[i]).append(' ');
    return s.toString().trim();
  }
}
