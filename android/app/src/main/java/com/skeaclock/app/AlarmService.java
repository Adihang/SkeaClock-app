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

  static boolean stopForClockInteraction(Context context) {
    if (activeId == -1) return false;
    return context.getApplicationContext().stopService(new Intent(context, AlarmService.class));
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
        Context localized = LocaleHelper.onAttach(this);
        android.widget.Toast.makeText(
                localized,
                localized.getString(R.string.snooze_permission_needed),
                android.widget.Toast.LENGTH_LONG)
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
    Context localized = LocaleHelper.onAttach(this);
    NotificationManager nm = getSystemService(NotificationManager.class);
    NotificationChannel channel =
        new NotificationChannel(
            CHANNEL, localized.getString(R.string.tab_alarm), NotificationManager.IMPORTANCE_HIGH);
    channel.setSound(null, null);
    channel.setDescription(localized.getString(R.string.alarm_status));
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
            .setContentTitle(
                alarm.label.trim().isEmpty()
                    ? localized.getString(R.string.alarm_notification_title)
                    : alarm.label)
            .setContentText(localized.getString(R.string.alarm_notification_body, alarm.time()))
            .setCategory(Notification.CATEGORY_ALARM)
            .setVisibility(Notification.VISIBILITY_PUBLIC)
            .setOngoing(true)
            .setContentIntent(open)
            .setFullScreenIntent(open, true)
            .addAction(
                new Notification.Action.Builder(
                        null, localized.getString(R.string.snooze_action), action(this, "SNOOZE"))
                    .build())
            .addAction(
                new Notification.Action.Builder(
                        null, localized.getString(R.string.stop_alarm_action), action(this, "STOP"))
                    .build())
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
