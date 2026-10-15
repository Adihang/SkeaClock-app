package com.skeaclock.app;

import android.app.PendingIntent;
import android.appwidget.AppWidgetManager;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.media.MediaPlayer;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.view.View;
import android.widget.RemoteViews;
import java.util.HashMap;
import java.util.Map;

/** Renders widget interactions into RemoteViews without opening the app activity. */
public final class WidgetActionReceiver extends BroadcastReceiver {
  static final String ACTION_WIDGET_INTERACTION = "com.skeaclock.app.WIDGET_INTERACTION";
  static final String EXTRA_WIDGET_ID = "com.skeaclock.app.WIDGET_ID";
  static final String EXTRA_ACTION = "com.skeaclock.app.WIDGET_ACTION_TYPE";
  static final String EXTRA_X = "com.skeaclock.app.WIDGET_ACTION_X";
  static final String EXTRA_Y = "com.skeaclock.app.WIDGET_ACTION_Y";

  private static final int FRAME_SIZE = 480;
  private static final long FRAME_INTERVAL_MS = 60;
  private static final long CHEEK_PULL_HOLD_MS = 2000;
  private static final long MAX_ANIMATION_MS = 8000;
  private static final Handler MAIN = new Handler(Looper.getMainLooper());
  private static final Map<Integer, AnimationRun> ACTIVE = new HashMap<>();

  @Override
  public void onReceive(Context context, Intent intent) {
    if (intent == null || !ACTION_WIDGET_INTERACTION.equals(intent.getAction())) return;
    final PendingResult pending = goAsync();
    final Context appContext = context.getApplicationContext();
    final int widgetId = intent.getIntExtra(EXTRA_WIDGET_ID, AppWidgetManager.INVALID_APPWIDGET_ID);
    final String action = intent.getStringExtra(EXTRA_ACTION);
    final float x = intent.getFloatExtra(EXTRA_X, 0f);
    final float y = intent.getFloatExtra(EXTRA_Y, 0f);
    MAIN.post(() -> start(appContext, widgetId, action, x, y, pending));
  }

  private static void start(
      Context context, int widgetId, String action, float x, float y, PendingResult pending) {
    if (widgetId == AppWidgetManager.INVALID_APPWIDGET_ID
        || (!MainActivity.WIDGET_BONK.equals(action)
            && !MainActivity.WIDGET_CHEEK.equals(action))) {
      pending.finish();
      return;
    }
    AppWidgetManager manager = AppWidgetManager.getInstance(context);
    if (manager.getAppWidgetInfo(widgetId) == null) {
      pending.finish();
      return;
    }

    AnimationRun previous;
    synchronized (ACTIVE) {
      previous = ACTIVE.remove(widgetId);
    }
    if (previous != null) previous.finish(true);

    AnimationRun run = new AnimationRun(context, manager, widgetId, action, x, y, pending);
    synchronized (ACTIVE) {
      ACTIVE.put(widgetId, run);
    }
    run.begin();
  }

  private static final class AnimationRun implements Runnable {
    private final Context context;
    private final AppWidgetManager manager;
    private final int widgetId;
    private final String action;
    private final float x, y;
    private final PendingResult pending;
    private long startedAt;
    private ClockView clock;
    private MediaPlayer player;
    private boolean released, finished;

    AnimationRun(
        Context context,
        AppWidgetManager manager,
        int widgetId,
        String action,
        float x,
        float y,
        PendingResult pending) {
      this.context = context;
      this.manager = manager;
      this.widgetId = widgetId;
      this.action = action;
      this.x = x;
      this.y = y;
      this.pending = pending;
    }

    void begin() {
      try {
        clock = new ClockView(context, action);
        clock.setFace(
            Store.prefs(context).getBoolean("background", true),
            Store.prefs(context).getBoolean("ticks", true),
            Store.prefs(context).getBoolean("numbers", true));
        clock.setEffects(this::playEffect);
        int measure = View.MeasureSpec.makeMeasureSpec(FRAME_SIZE, View.MeasureSpec.EXACTLY);
        clock.measure(measure, measure);
        clock.layout(0, 0, FRAME_SIZE, FRAME_SIZE);
        if (MainActivity.WIDGET_BONK.equals(action)) clock.triggerSmash(x, y);
        else clock.triggerWidgetCheekPull(x, y);
        startedAt = SystemClock.uptimeMillis();
        drawFrame();
      } catch (RuntimeException error) {
        android.util.Log.e("SkeaClock", "Widget animation failed", error);
        finish(true);
      }
    }

    @Override
    public void run() {
      try {
        drawFrame();
      } catch (RuntimeException error) {
        android.util.Log.e("SkeaClock", "Widget animation frame failed", error);
        finish(true);
      }
    }

    private void drawFrame() {
      if (finished) return;
      long now = SystemClock.uptimeMillis(), elapsed = now - startedAt;
      if (!MainActivity.WIDGET_BONK.equals(action)
          && !released
          && elapsed >= CHEEK_PULL_HOLD_MS) {
        clock.finishWidgetCheekPull();
        released = true;
      }

      Bitmap frame = Bitmap.createBitmap(FRAME_SIZE, FRAME_SIZE, Bitmap.Config.ARGB_8888);
      clock.drawWidgetFrame(new Canvas(frame));
      RemoteViews update = new RemoteViews(context.getPackageName(), R.layout.widget_clock);
      update.setViewVisibility(R.id.dial, View.INVISIBLE);
      update.setViewVisibility(R.id.hands, View.INVISIBLE);
      update.setImageViewBitmap(R.id.widget_action_frame, frame);
      update.setViewVisibility(R.id.widget_action_frame, View.VISIBLE);
      try {
        manager.partiallyUpdateAppWidget(widgetId, update);
      } catch (RuntimeException error) {
        android.util.Log.w("SkeaClock", "Widget was removed during animation", error);
        finish(false);
        return;
      }

      if (("IDLE".equals(clock.modeName()) && elapsed > 300)
          || elapsed >= MAX_ANIMATION_MS) {
        finish(true);
      } else {
        MAIN.postDelayed(this, FRAME_INTERVAL_MS);
      }
    }

    private void playEffect(String name) {
      int resource = context.getResources().getIdentifier(name, "raw", context.getPackageName());
      if (resource == 0) return;
      releasePlayer();
      player = MediaPlayer.create(context, resource);
      if (player == null) return;
      float volume = Store.prefs(context).getInt("effects", 80) / 100f;
      player.setVolume(volume, volume);
      player.setOnCompletionListener(
          completed -> {
            completed.release();
            if (player == completed) player = null;
          });
      player.start();
    }

    void finish(boolean restoreWidget) {
      if (finished) return;
      finished = true;
      MAIN.removeCallbacks(this);
      try {
        if (restoreWidget && manager.getAppWidgetInfo(widgetId) != null) {
          RemoteViews restore = new RemoteViews(context.getPackageName(), R.layout.widget_clock);
          restore.setViewVisibility(R.id.dial, View.VISIBLE);
          restore.setViewVisibility(R.id.hands, View.VISIBLE);
          restore.setViewVisibility(R.id.widget_action_frame, View.GONE);
          manager.partiallyUpdateAppWidget(widgetId, restore);
        }
      } catch (RuntimeException error) {
        android.util.Log.w("SkeaClock", "Could not restore static widget", error);
      } finally {
        releasePlayer();
        synchronized (ACTIVE) {
          if (ACTIVE.get(widgetId) == this) ACTIVE.remove(widgetId);
        }
        pending.finish();
      }
    }

    private void releasePlayer() {
      if (player == null) return;
      player.setOnCompletionListener(null);
      player.release();
      player = null;
    }
  }
}
