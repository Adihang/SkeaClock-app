package com.skeaclock.app;

import android.app.PendingIntent;
import android.appwidget.AppWidgetManager;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.media.MediaPlayer;
import android.os.HandlerThread;
import android.os.Handler;
import android.os.Looper;
import android.os.Process;
import android.os.SystemClock;
import android.view.View;
import android.widget.RemoteViews;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/** Renders widget interactions into RemoteViews without opening the app activity. */
public final class WidgetActionReceiver extends BroadcastReceiver {
  static final String ACTION_WIDGET_INTERACTION = "com.skeaclock.app.WIDGET_INTERACTION";
  static final String EXTRA_WIDGET_ID = "com.skeaclock.app.WIDGET_ID";
  static final String EXTRA_ACTION = "com.skeaclock.app.WIDGET_ACTION_TYPE";
  static final String EXTRA_X = "com.skeaclock.app.WIDGET_ACTION_X";
  static final String EXTRA_Y = "com.skeaclock.app.WIDGET_ACTION_Y";

  private static final int VIEW_SIZE = 480;
  private static final int FRAME_SIZE = 400;
  private static final long FRAME_INTERVAL_MS = 50;
  private static final long CHEEK_PULL_HOLD_MS = 2000;
  private static final long MAX_ANIMATION_MS = 8000;
  private static final long REPEAT_TAP_COOLDOWN_MS = 500;
  private static final HandlerThread RENDER_THREAD = new HandlerThread("SkeaWidgetRender");
  private static final HandlerThread AUDIO_THREAD = new HandlerThread("SkeaWidgetAudio");
  private static final HandlerThread ASSET_THREAD =
      new HandlerThread("SkeaWidgetAssets", Process.THREAD_PRIORITY_BACKGROUND);
  private static final Handler RENDER;
  private static final Handler AUDIO;
  private static final Handler ASSETS;
  private static final Map<Integer, AnimationRun> ACTIVE = new HashMap<>();
  private static final Set<Integer> BUSY_WIDGETS = new HashSet<>();

  static {
    RENDER_THREAD.start();
    AUDIO_THREAD.start();
    ASSET_THREAD.start();
    RENDER = new Handler(RENDER_THREAD.getLooper());
    AUDIO = new Handler(AUDIO_THREAD.getLooper());
    ASSETS = new Handler(ASSET_THREAD.getLooper());
  }

  @Override
  public void onReceive(Context context, Intent intent) {
    if (intent == null || !ACTION_WIDGET_INTERACTION.equals(intent.getAction())) return;
    final PendingResult pending = goAsync();
    final Context appContext = context.getApplicationContext();
    final int widgetId = intent.getIntExtra(EXTRA_WIDGET_ID, AppWidgetManager.INVALID_APPWIDGET_ID);
    final String action = intent.getStringExtra(EXTRA_ACTION);
    final float x = intent.getFloatExtra(EXTRA_X, 0f);
    final float y = intent.getFloatExtra(EXTRA_Y, 0f);
    if (widgetId == AppWidgetManager.INVALID_APPWIDGET_ID
        || (!MainActivity.WIDGET_BONK.equals(action)
            && !MainActivity.WIDGET_CHEEK.equals(action))) {
      pending.finish();
      return;
    }

    // Reserve the widget before posting to the render thread so taps already queued by Android
    // cannot start another animation after the current one finishes.
    boolean reserved;
    synchronized (ACTIVE) {
      reserved = BUSY_WIDGETS.add(widgetId);
    }
    if (!reserved) {
      pending.finish();
      return;
    }
    if (!RENDER.post(() -> start(appContext, widgetId, action, x, y, pending))) {
      releaseWidget(widgetId);
      pending.finish();
    }
  }

  private static void start(
      Context context, int widgetId, String action, float x, float y, PendingResult pending) {
    if (widgetId == AppWidgetManager.INVALID_APPWIDGET_ID
        || (!MainActivity.WIDGET_BONK.equals(action)
            && !MainActivity.WIDGET_CHEEK.equals(action))) {
      releaseWidget(widgetId);
      pending.finish();
      return;
    }
    final AppWidgetManager manager;
    try {
      manager = AppWidgetManager.getInstance(context);
      if (manager.getAppWidgetInfo(widgetId) == null) {
        releaseWidget(widgetId);
        pending.finish();
        return;
      }
    } catch (RuntimeException error) {
      android.util.Log.e("SkeaClock", "Could not prepare widget animation", error);
      releaseWidget(widgetId);
      pending.finish();
      return;
    }

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
    private Bitmap frameBitmap;
    private Canvas frameCanvas;
    private MediaPlayer player;
    private boolean firstFrame = true, released, finished;

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
        int measure = View.MeasureSpec.makeMeasureSpec(VIEW_SIZE, View.MeasureSpec.EXACTLY);
        clock.measure(measure, measure);
        clock.layout(0, 0, VIEW_SIZE, VIEW_SIZE);
        frameBitmap = Bitmap.createBitmap(FRAME_SIZE, FRAME_SIZE, Bitmap.Config.ARGB_8888);
        frameCanvas = new Canvas(frameBitmap);
        float scale = FRAME_SIZE / (float) VIEW_SIZE;
        frameCanvas.scale(scale, scale);
        if (MainActivity.WIDGET_BONK.equals(action)) clock.triggerSmash(x, y);
        else clock.triggerWidgetCheekPull(x, y);
        startedAt = SystemClock.uptimeMillis();
        drawFrame();
        if (!finished) ASSETS.post(() -> clock.preloadWidgetAnimationRemainder(action));
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

      frameBitmap.eraseColor(Color.TRANSPARENT);
      clock.drawWidgetFrame(frameCanvas);
      RemoteViews update = new RemoteViews(context.getPackageName(), R.layout.widget_clock);
      if (firstFrame) {
        update.setViewVisibility(R.id.widget_idle_character, View.VISIBLE);
        update.setViewVisibility(R.id.dial_markers, View.INVISIBLE);
        update.setViewVisibility(R.id.hands, View.INVISIBLE);
        firstFrame = false;
      }
      update.setImageViewBitmap(R.id.widget_idle_character, frameBitmap);
      try {
        manager.partiallyUpdateAppWidget(widgetId, update);
      } catch (RuntimeException error) {
        android.util.Log.w("SkeaClock", "Widget was removed during animation", error);
        finish(false);
        return;
      }

      if (clock.animationComplete() || elapsed >= MAX_ANIMATION_MS) {
        finish(true);
      } else {
        RENDER.postDelayed(this, FRAME_INTERVAL_MS);
      }
    }

    private void playEffect(String name) {
      AUDIO.post(
          () -> {
            int resource =
                context.getResources().getIdentifier(name, "raw", context.getPackageName());
            if (resource == 0) return;
            releasePlayerNow();
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
          });
    }

    void finish(boolean restoreWidget) {
      if (finished) return;
      finished = true;
      RENDER.removeCallbacks(this);
      try {
        if (restoreWidget && manager.getAppWidgetInfo(widgetId) != null) {
          RemoteViews restore = new RemoteViews(context.getPackageName(), R.layout.widget_clock);
          restore.setImageViewResource(R.id.dial, Store.dialBackground(context));
          restore.setImageViewResource(R.id.dial_markers, Store.dialMarkers(context));
          restore.setImageViewResource(R.id.widget_idle_character, R.drawable.widget_idle);
          restore.setViewVisibility(R.id.dial_markers, View.VISIBLE);
          restore.setViewVisibility(R.id.widget_idle_character, View.VISIBLE);
          restore.setViewVisibility(R.id.hands, View.VISIBLE);
          manager.partiallyUpdateAppWidget(widgetId, restore);
        }
      } catch (RuntimeException error) {
        android.util.Log.w("SkeaClock", "Could not restore static widget", error);
      } finally {
        releasePlayer();
        synchronized (ACTIVE) {
          if (ACTIVE.get(widgetId) == this) ACTIVE.remove(widgetId);
        }
        if (!RENDER.postDelayed(() -> releaseWidget(widgetId), REPEAT_TAP_COOLDOWN_MS)) {
          releaseWidget(widgetId);
        }
        pending.finish();
      }
    }

    private void releasePlayer() {
      AUDIO.post(this::releasePlayerNow);
    }

    private void releasePlayerNow() {
      if (player == null) return;
      player.setOnCompletionListener(null);
      player.release();
      player = null;
    }
  }

  private static void releaseWidget(int widgetId) {
    synchronized (ACTIVE) {
      BUSY_WIDGETS.remove(widgetId);
    }
  }
}
