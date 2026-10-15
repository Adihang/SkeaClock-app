package com.skeaclock.app;

import android.app.*;
import android.content.*;
import android.media.*;
import android.net.Uri;
import android.os.*;

public class AlarmService extends Service {
  static final String CHANNEL = "alarms", CLOSED = "com.skeaclock.app.RING_CLOSED";
  static volatile int activeId = -1;
  private MediaPlayer player;
  private Vibrator vibrator;
  private PowerManager.WakeLock wakeLock;
  private AudioFocusRequest focus;
  private final Handler handler = new Handler(Looper.getMainLooper());

  @Override
  public IBinder onBind(Intent intent) {
    return null;
  }

  @Override
  public int onStartCommand(Intent intent, int flags, int startId) {
    if (intent == null) {
      stopSelf();
      return START_NOT_STICKY;
    }
    String action = intent.getAction();
    if ("STOP".equals(action) || "SNOOZE".equals(action)) {
      if ("SNOOZE".equals(action) && activeId != -1 && !Scheduler.snooze(this, activeId)) {
        android.widget.Toast.makeText(
                this, "다시 울리려면 정확한 알람 권한이 필요합니다", android.widget.Toast.LENGTH_LONG)
            .show();
        return START_NOT_STICKY;
      }
      stopSelf();
      return START_NOT_STICKY;
    }
    activeId = intent.getIntExtra("id", -1);
    Alarm alarm = Store.find(this, activeId);
    if (alarm == null) {
      stopSelf();
      return START_NOT_STICKY;
    }
    NotificationManager nm = getSystemService(NotificationManager.class);
    NotificationChannel channel =
        new NotificationChannel(CHANNEL, "알람", NotificationManager.IMPORTANCE_HIGH);
    channel.setSound(null, null);
    channel.setDescription("설정한 시간의 알람과 해제 버튼");
    nm.createNotificationChannel(channel);
    PendingIntent open =
        PendingIntent.getActivity(
            this,
            activeId,
            new Intent(this, RingActivity.class).putExtra("id", activeId),
            PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
    Notification notification =
        new Notification.Builder(this, CHANNEL)
            .setSmallIcon(R.drawable.ic_clock)
            .setContentTitle(alarm.label.trim().isEmpty() ? "스키아 알람" : alarm.label)
            .setContentText(alarm.time() + " · 일어날 시간이에요")
            .setCategory(Notification.CATEGORY_ALARM)
            .setVisibility(Notification.VISIBILITY_PUBLIC)
            .setOngoing(true)
            .setContentIntent(open)
            .setFullScreenIntent(open, true)
            .addAction(
                new Notification.Action.Builder(null, "5분 후 다시", action(this, "SNOOZE")).build())
            .addAction(new Notification.Action.Builder(null, "알람 끄기", action(this, "STOP")).build())
            .build();
    startForeground(1001, notification);
    releaseAudio();
    wakeLock =
        getSystemService(PowerManager.class)
            .newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "SkeaClock:alarm");
    wakeLock.acquire(10 * 60_000L);
    AudioAttributes attrs =
        new AudioAttributes.Builder()
            .setUsage(AudioAttributes.USAGE_ALARM)
            .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
            .build();
    focus =
        new AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN_TRANSIENT)
            .setAudioAttributes(attrs)
            .setOnAudioFocusChangeListener(change -> {})
            .build();
    getSystemService(AudioManager.class).requestAudioFocus(focus);
    String custom = Store.prefs(this).getString("sound", "");
    try {
      play(
          custom.isEmpty()
              ? Uri.parse("android.resource://" + getPackageName() + "/" + R.raw.base_alert)
              : Uri.parse(custom),
          attrs);
    } catch (Exception e) {
      try {
        play(Uri.parse("android.resource://" + getPackageName() + "/" + R.raw.base_alert), attrs);
      } catch (Exception fallback) {
        android.util.Log.e("SkeaClock", "Cannot play alarm", fallback);
      }
    }
    if (Store.prefs(this).getBoolean("vibration", true)) {
      vibrator = getSystemService(Vibrator.class);
      vibrator.vibrate(VibrationEffect.createWaveform(new long[] {0, 500, 500}, 0), attrs);
    }
    handler.removeCallbacksAndMessages(null);
    handler.postDelayed(this::stopSelf, 10 * 60_000L);
    return START_NOT_STICKY;
  }

  private void play(Uri uri, AudioAttributes attrs) throws Exception {
    if (player != null) player.release();
    player = new MediaPlayer();
    player.setAudioAttributes(attrs);
    player.setDataSource(this, uri);
    player.setLooping(true);
    float volume = Store.prefs(this).getInt("volume", 80) / 100f;
    player.setVolume(volume, volume);
    player.prepare();
    player.start();
  }

  static PendingIntent action(Context c, String action) {
    return PendingIntent.getService(
        c,
        action.hashCode(),
        new Intent(c, AlarmService.class).setAction(action),
        PendingIntent.FLAG_IMMUTABLE);
  }

  private void releaseAudio() {
    if (player != null) {
      player.release();
      player = null;
    }
    if (vibrator != null) {
      vibrator.cancel();
      vibrator = null;
    }
    if (wakeLock != null && wakeLock.isHeld()) wakeLock.release();
    if (focus != null) getSystemService(AudioManager.class).abandonAudioFocusRequest(focus);
  }

  @Override
  public void onDestroy() {
    handler.removeCallbacksAndMessages(null);
    releaseAudio();
    activeId = -1;
    stopForeground(STOP_FOREGROUND_REMOVE);
    sendBroadcast(new Intent(CLOSED).setPackage(getPackageName()));
    super.onDestroy();
  }
}
