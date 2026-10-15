package com.skeaclock.app;

import android.app.*;
import android.content.*;
import android.os.*;
import android.widget.*;

/** Run on a disposable emulator: clears this app's saved test data. */
public class SmokeInstrumentation extends Instrumentation {
  @Override
  public void onCreate(Bundle args) {
    super.onCreate(args);
    start();
  }

  private void check(boolean value, String message) {
    if (!value) throw new AssertionError(message);
  }

  @Override
  public void onStart() {
    Bundle result = new Bundle();
    Context c = getTargetContext();
    try {
      Store.prefs(c).edit().clear().commit();
      check(Scheduler.allowed(c), "Grant exact alarm app-op before running");
      Alarm a = new Alarm(Store.nextId(c), 7, 30, 31, true, "Instrumentation alarm");
      Store.save(c, a);
      check(Store.find(c, a.id).days == 31, "Alarm persistence");
      check(Scheduler.schedule(c, a), "Weekly schedule");
      check(Scheduler.manager(c).getNextAlarmClock() != null, "AlarmManager registered alarm");
      a.days = 0;
      Store.save(c, a);
      check(
          Scheduler.at(c, a.id, System.currentTimeMillis() + 2000, false), "Immediate exact alarm");
      long deadline = SystemClock.elapsedRealtime() + 15000;
      while (AlarmService.activeId != a.id && SystemClock.elapsedRealtime() < deadline)
        SystemClock.sleep(100);
      check(AlarmService.activeId == a.id, "Receiver launched foreground alarm service");
      check(!Store.find(c, a.id).enabled, "One-shot disabled after delivery");
      SystemClock.sleep(500);
      c.startService(new Intent(c, AlarmService.class).setAction("SNOOZE"));
      deadline = SystemClock.elapsedRealtime() + 5000;
      while (AlarmService.activeId != -1 && SystemClock.elapsedRealtime() < deadline)
        SystemClock.sleep(100);
      check(AlarmService.activeId == -1, "Snooze stopped playback service");
      check(
          Store.prefs(c).getLong("snooze_" + a.id, 0) > System.currentTimeMillis(),
          "Snooze persisted");
      Scheduler.restore(c);
      check(Scheduler.manager(c).getNextAlarmClock() != null, "Restore snooze");
      Scheduler.cancel(c, a.id);
      Store.delete(c, a.id);
      check(Store.alarms(c).isEmpty(), "Delete persisted alarm");
      runOnMainSync(
          () -> {
            RemoteViews views = new RemoteViews(c.getPackageName(), R.layout.widget_clock);
            views.setImageViewResource(R.id.dial, Store.dial(c));
            android.view.View widget = views.apply(c, new FrameLayout(c));
            check(
                widget.findViewById(R.id.hands) instanceof AnalogClock,
                "Widget inflated live AnalogClock");
            check(
                widget.findViewById(R.id.widget_action_frame).getVisibility()
                    == android.view.View.GONE,
                "Widget action frame starts hidden");
            for (int flags = 0; flags < 8; flags++) {
              Store.prefs(c)
                  .edit()
                  .putBoolean("background", (flags & 1) != 0)
                  .putBoolean("ticks", (flags & 2) != 0)
                  .putBoolean("numbers", (flags & 4) != 0)
                  .commit();
              check(c.getDrawable(Store.dial(c)) != null, "Dial variant " + flags);
            }
          });
      result.putString(
          "stream",
          "PASS: persistence, weekly registration, exact alarm delivery, foreground playback"
              + " service, one-shot completion, snooze, restore, delete, RemoteViews AnalogClock"
              + " and 8 dial variants\n");
      finish(Activity.RESULT_OK, result);
    } catch (Throwable e) {
      result.putString("stream", android.util.Log.getStackTraceString(e));
      finish(Activity.RESULT_CANCELED, result);
    } finally {
      c.stopService(new Intent(c, AlarmService.class));
      for (Alarm a : Store.alarms(c)) Scheduler.cancel(c, a.id);
      Store.prefs(c).edit().clear().commit();
    }
  }
}
