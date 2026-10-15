package com.skeaclock.app;

import android.content.*;

public class AlarmReceiver extends BroadcastReceiver {
  @Override
  public void onReceive(Context c, Intent i) {
    int id = i.getIntExtra("id", -1);
    Alarm a = Store.find(c, id);
    boolean snooze = "SNOOZE".equals(i.getAction());
    if (a == null || (!snooze && !a.enabled)) return;
    if (snooze) Store.prefs(c).edit().remove("snooze_" + id).commit();
    else if (a.days == 0) {
      a.enabled = false;
      Store.save(c, a);
    } else Scheduler.schedule(c, a);
    c.startForegroundService(new Intent(c, AlarmService.class).putExtra("id", id));
  }
}
