package com.skeaclock.app;

import android.app.*;
import android.appwidget.AppWidgetManager;
import android.content.*;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.os.*;
import android.widget.*;
import java.lang.reflect.Field;
import java.util.Map;

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
            views.setImageViewResource(R.id.dial, Store.dialBackground(c));
            views.setImageViewResource(R.id.dial_markers, Store.dialMarkers(c));
            android.view.View widget = views.apply(c, new FrameLayout(c));
            int exactSize = android.view.View.MeasureSpec.makeMeasureSpec(480, android.view.View.MeasureSpec.EXACTLY);
            widget.measure(exactSize, exactSize);
            widget.layout(0, 0, 480, 480);
            check(
                widget.findViewById(R.id.hands) instanceof AnalogClock,
                "Widget inflated live AnalogClock");
            ImageView dial = widget.findViewById(R.id.dial);
            check(
                dial.getVisibility() == android.view.View.VISIBLE && dial.getDrawable() != null,
                "Widget background layer is visible");
            ImageView idleCharacter = widget.findViewById(R.id.widget_idle_character);
            check(
                idleCharacter.getDrawable() != null,
                "Widget idle character layer has its fallback image");
            ImageView markers = widget.findViewById(R.id.dial_markers);
            check(
                markers.getDrawable() != null
                    && ((FrameLayout) widget.findViewById(R.id.widget_dial_area))
                            .indexOfChild(idleCharacter)
                        < ((FrameLayout) widget.findViewById(R.id.widget_dial_area))
                            .indexOfChild(markers),
                "Widget character stays behind dial markers");
            android.view.View cheekTarget = widget.findViewById(R.id.widget_r2c0);
            check(
                cheekTarget.getWidth() >= 80 && cheekTarget.getHeight() >= 80,
                "Weighted cheek touch target fills its grid cell");
            if (Build.VERSION.SDK_INT >= 28) {
              check(
                  idleCharacter.getDrawable()
                      instanceof android.graphics.drawable.AnimatedImageDrawable,
                  "Widget idle character uses a native animated drawable");
            }
            for (int flags = 0; flags < 8; flags++) {
              Store.prefs(c)
                  .edit()
                  .putBoolean("background", (flags & 1) != 0)
                  .putBoolean("ticks", (flags & 2) != 0)
                  .putBoolean("numbers", (flags & 4) != 0)
                  .commit();
              check(
                  c.getDrawable(Store.dialBackground(c)) != null
                      && c.getDrawable(Store.dialMarkers(c)) != null,
                  "Dial background and marker variants " + flags);
            }
          });
      checkWidgetAnimationStartup(c, MainActivity.WIDGET_BONK);
      checkWidgetAnimationStartup(c, MainActivity.WIDGET_CHEEK);
      boolean widgetTapBurstChecked = checkRepeatedWidgetTaps(c);
      result.putString(
          "stream",
          "PASS: persistence, weekly registration, exact alarm delivery, foreground playback"
              + " service, one-shot completion, snooze, restore, delete, RemoteViews AnalogClock"
              + " with 8 dial variants, layered clock face, full-cell touch targets, and API 28+"
              + " widget idle animation resource, action-first widget frames, and deferred follow-up"
              + " animation decoding"
              + (widgetTapBurstChecked
                  ? "; repeated widget taps remain one animation\n"
                  : "; widget tap burst check skipped because no widget is bound\n"));
      finish(Activity.RESULT_OK, result);
    } catch (Throwable e) {
      result.putString("stream", android.util.Log.getStackTraceString(e));
      finish(Activity.RESULT_CANCELED, result);
    } finally {
      c.stopService(new Intent(c, AlarmService.class));
      for (Alarm a : Store.alarms(c)) Scheduler.cancel(c, a.id);
      Store.prefs(c).edit().clear().commit();
      ClockWidget.refresh(c);
    }
  }

  private void checkWidgetAnimationStartup(Context context, String action) {
    ClockView clock = new ClockView(context, action);
    int exactSize = android.view.View.MeasureSpec.makeMeasureSpec(480, android.view.View.MeasureSpec.EXACTLY);
    clock.measure(exactSize, exactSize);
    clock.layout(0, 0, 480, 480);
    if (MainActivity.WIDGET_BONK.equals(action)) clock.triggerSmash(240, 180);
    else clock.triggerWidgetCheekPull(1, 0);
    Bitmap firstFrame = Bitmap.createBitmap(400, 400, Bitmap.Config.ARGB_8888);
    clock.drawWidgetFrame(new Canvas(firstFrame));

    if (MainActivity.WIDGET_BONK.equals(action)) {
      check(clock.isWidgetAnimationClipLoaded("skea_smash"), "Bonk's first clip loads immediately");
      check(
          !clock.isWidgetAnimationClipLoaded("skea_angry")
              && !clock.isWidgetAnimationClipLoaded("skea_angry_alt"),
          "Bonk reaction clips remain deferred until its first frame is rendered");
    } else {
      check(
          clock.isWidgetAnimationClipLoaded("skea_touch_back")
              && clock.isWidgetAnimationClipLoaded("skea_touch_head")
              && clock.isWidgetAnimationClipLoaded("skea_touch_front"),
          "Cheek pull's first clips load immediately");
      check(
          !clock.isWidgetAnimationClipLoaded("skea_touch_end")
              && !clock.isWidgetAnimationClipLoaded("skea_touch_end_back"),
          "Cheek release clips remain deferred while the pull is held");
    }

    clock.preloadWidgetAnimationRemainder(action);
    if (MainActivity.WIDGET_BONK.equals(action)) {
      check(
          clock.isWidgetAnimationClipLoaded("skea_angry")
              && clock.isWidgetAnimationClipLoaded("skea_angry_alt")
              && clock.isWidgetAnimationClipLoaded("skea_transition"),
          "Bonk's remaining clips can be preloaded");
    } else {
      check(
          clock.isWidgetAnimationClipLoaded("skea_touch_end")
              && clock.isWidgetAnimationClipLoaded("skea_touch_end_back")
              && clock.isWidgetAnimationClipLoaded("skea_touch_end_head")
              && clock.isWidgetAnimationClipLoaded("skea_touch_end_front"),
          "Cheek release clips can be preloaded");
    }
    firstFrame.recycle();
  }

  private boolean checkRepeatedWidgetTaps(Context context) {
    int[] widgetIds =
        AppWidgetManager.getInstance(context)
            .getAppWidgetIds(new ComponentName(context, ClockWidget.class));
    if (widgetIds.length == 0) return false;

    int widgetId = widgetIds[0];
    Intent tap =
        new Intent(context, WidgetActionReceiver.class)
            .setAction(WidgetActionReceiver.ACTION_WIDGET_INTERACTION)
            .putExtra(WidgetActionReceiver.EXTRA_WIDGET_ID, widgetId)
            .putExtra(WidgetActionReceiver.EXTRA_ACTION, MainActivity.WIDGET_CHEEK)
            .putExtra(WidgetActionReceiver.EXTRA_X, 240f)
            .putExtra(WidgetActionReceiver.EXTRA_Y, 200f);
    for (int i = 0; i < 10; i++) context.sendBroadcast(new Intent(tap));

    long deadline = SystemClock.elapsedRealtime() + 3000;
    Object firstRun = activeRun(widgetId);
    while (firstRun == null && SystemClock.elapsedRealtime() < deadline) {
      SystemClock.sleep(20);
      firstRun = activeRun(widgetId);
    }
    check(firstRun != null, "Widget interaction starts an animation");

    SystemClock.sleep(200);
    for (int i = 0; i < 10; i++) context.sendBroadcast(new Intent(tap));
    SystemClock.sleep(200);
    check(activeRun(widgetId) == firstRun, "Repeated widget taps do not restart the animation");

    deadline = SystemClock.elapsedRealtime() + 10000;
    while (activeRun(widgetId) != null && SystemClock.elapsedRealtime() < deadline)
      SystemClock.sleep(50);
    check(activeRun(widgetId) == null, "Widget interaction animation completes");
    SystemClock.sleep(1500);
    check(activeRun(widgetId) == null, "Queued widget taps do not replay after completion");
    return true;
  }

  @SuppressWarnings("unchecked")
  private Object activeRun(int widgetId) {
    final Object[] value = new Object[1];
    final Throwable[] failure = new Throwable[1];
    runOnMainSync(
        () -> {
          try {
            Field field = WidgetActionReceiver.class.getDeclaredField("ACTIVE");
            field.setAccessible(true);
            Map<Integer, Object> active = (Map<Integer, Object>) field.get(null);
            synchronized (active) {
              value[0] = active.get(widgetId);
            }
          } catch (Throwable error) {
            failure[0] = error;
          }
        });
    if (failure[0] != null) throw new AssertionError("Could not inspect widget animation", failure[0]);
    return value[0];
  }
}
