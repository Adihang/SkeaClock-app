package com.skeaclock.app;

import android.app.*;
import android.content.*;
import android.os.Build;
import java.time.ZoneId;

final class Scheduler {
  static AlarmManager manager(Context c) {
    return c.getSystemService(AlarmManager.class);
  }

  static boolean allowed(Context c) {
    return Build.VERSION.SDK_INT < 31 || manager(c).canScheduleExactAlarms();
  }

  static PendingIntent operation(Context c, int id, boolean snooze) {
    Intent i =
        new Intent(c, AlarmReceiver.class).setAction(snooze ? "SNOOZE" : "FIRE").putExtra("id", id);
    return PendingIntent.getBroadcast(
        c, id, i, PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
  }

  static void cancel(Context c, int id) {
    manager(c).cancel(operation(c, id, false));
    manager(c).cancel(operation(c, id, true));
    Store.prefs(c).edit().remove("snooze_" + id).commit();
  }

  static boolean schedule(Context c, Alarm a) {
    manager(c).cancel(operation(c, a.id, false));
    if (!a.enabled) return true;
    return at(c, a.id, a.next(System.currentTimeMillis(), ZoneId.systemDefault()), false);
  }

  static boolean at(Context c, int id, long time, boolean snooze) {
    if (!allowed(c)) return false;
    PendingIntent show =
        PendingIntent.getActivity(
            c,
            id,
            new Intent(c, MainActivity.class),
            PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
    try {
      manager(c)
          .setAlarmClock(new AlarmManager.AlarmClockInfo(time, show), operation(c, id, snooze));
      return true;
    } catch (SecurityException e) {
      return false;
    }
  }

  static boolean snooze(Context c, int id) {
    long time = System.currentTimeMillis() + 5 * 60_000;
    if (!at(c, id, time, true)) return false;
    Store.prefs(c).edit().putLong("snooze_" + id, time).commit();
    return true;
  }

  static void restore(Context c) {
    for (Alarm a : Store.alarms(c)) {
      schedule(c, a);
      long time = Store.prefs(c).getLong("snooze_" + a.id, 0);
      if (time > 0) at(c, a.id, Math.max(time, System.currentTimeMillis() + 1000), true);
    }
  }
}
