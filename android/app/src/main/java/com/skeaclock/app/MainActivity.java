package com.skeaclock.app;

import android.Manifest;
import android.app.*;
import android.appwidget.AppWidgetManager;
import android.content.*;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.media.MediaPlayer;
import android.net.Uri;
import android.os.*;
import android.provider.Settings;
import android.view.*;
import android.widget.*;
import java.time.*;
import java.time.format.DateTimeFormatter;

public class MainActivity extends Activity {
  static final String WIDGET_BONK = "bonk";
  static final String WIDGET_CHEEK = "cheek";
  static final String WIDGET_PAT = "pat";
  private LinearLayout root, body, nav;
  private int tab = 0;
  private boolean resumed;
  private ClockView clock;
  private MediaPlayer effect;
  private TextView digital, date;
  private final Handler handler = new Handler(Looper.getMainLooper());
  private final Runnable tick =
      new Runnable() {
        public void run() {
          if (digital != null) {
            ZonedDateTime now = ZonedDateTime.now();
            digital.setText(
                now.format(
                    DateTimeFormatter.ofPattern(
                        Store.prefs(MainActivity.this).getBoolean("24hour", true)
                            ? "HH:mm"
                            : "a h:mm",
                        LocaleHelper.locale(MainActivity.this))));
            date.setText(
                now.format(
                    DateTimeFormatter.ofPattern(
                        getString(R.string.date_format), LocaleHelper.locale(MainActivity.this))));
          }
          handler.postDelayed(this, 1000);
        }
      };

  @Override
  public void onCreate(Bundle state) {
    super.onCreate(state);
    if (state != null) tab = state.getInt("tab");
    root = new LinearLayout(this);
    root.setOrientation(LinearLayout.VERTICAL);
    root.setBackgroundColor(0xfff7f9f5);
    root.setOnApplyWindowInsetsListener(
        (v, insets) -> {
          v.setPadding(
              insets.getSystemWindowInsetLeft(),
              insets.getSystemWindowInsetTop(),
              insets.getSystemWindowInsetRight(),
              insets.getSystemWindowInsetBottom());
          return insets;
        });
    setContentView(root);
    root.requestApplyInsets();
    render();
  }

  @Override
  protected void attachBaseContext(Context newBase) {
    super.attachBaseContext(LocaleHelper.onAttach(newBase));
  }

  @Override
  protected void onSaveInstanceState(Bundle b) {
    b.putInt("tab", tab);
    super.onSaveInstanceState(b);
  }

  @Override
  protected void onResume() {
    super.onResume();
    resumed = true;
    Scheduler.restore(this);
    render();
    handler.removeCallbacks(tick);
    handler.post(tick);
  }

  @Override
  protected void onNewIntent(Intent intent) {
    super.onNewIntent(intent);
    setIntent(intent);
    tab = 0;
    if (resumed) render();
  }

  @Override
  protected void onPause() {
    resumed = false;
    handler.removeCallbacks(tick);
    if (effect != null) {
      effect.release();
      effect = null;
    }
    super.onPause();
  }

  private int dp(int n) {
    return Math.round(n * getResources().getDisplayMetrics().density);
  }

  private TextView text(String value, int size, int color) {
    TextView t = new TextView(this);
    t.setText(value);
    t.setTextSize(size);
    t.setTextColor(color);
    t.setPadding(0, dp(6), 0, dp(6));
    return t;
  }

  private TextView heading(String value, int size) {
    TextView t = text(value, size, 0xff233b35);
    t.setTypeface(null, Typeface.BOLD);
    return t;
  }

  private Button button(String label, Runnable action) {
    Button b = new Button(this);
    b.setText(label);
    b.setAllCaps(false);
    b.setTextColor(0xff245e54);
    b.setTextSize(15);
    b.setMinHeight(dp(48));
    b.setPadding(dp(16), dp(10), dp(16), dp(10));
    b.setStateListAnimator(null);
    GradientDrawable shape = new GradientDrawable();
    shape.setColor(0xffe0eee7);
    shape.setCornerRadius(dp(16));
    b.setBackground(
        new android.graphics.drawable.RippleDrawable(
            android.content.res.ColorStateList.valueOf(0x2229796e), shape, null));
    b.setOnClickListener(v -> action.run());
    LinearLayout.LayoutParams p = new LinearLayout.LayoutParams(-1, -2);
    p.setMargins(0, dp(6), 0, dp(6));
    b.setLayoutParams(p);
    return b;
  }

  private LinearLayout card() {
    LinearLayout c = new LinearLayout(this);
    c.setOrientation(LinearLayout.VERTICAL);
    c.setPadding(dp(20), dp(14), dp(20), dp(14));
    GradientDrawable bg = new GradientDrawable();
    bg.setColor(Color.WHITE);
    bg.setCornerRadius(dp(24));
    c.setBackground(bg);
    LinearLayout.LayoutParams p = new LinearLayout.LayoutParams(-1, -2);
    p.setMargins(0, dp(10), 0, dp(8));
    body.addView(c, p);
    return c;
  }

  private void render() {
    root.removeAllViews();
    digital = null;
    date = null;
    clock = null;
    ScrollView scroll = new ScrollView(this);
    scroll.setFillViewport(true);
    body = new LinearLayout(this);
    body.setOrientation(LinearLayout.VERTICAL);
    body.setPadding(dp(24), dp(20), dp(24), dp(24));
    scroll.addView(body);
    root.addView(scroll, new LinearLayout.LayoutParams(-1, 0, 1));
    body.addView(text("S K E A   C L O C K", 12, 0xff29796e));
    if (tab == 0) clockPage();
    else if (tab == 1) alarmPage();
    else settingsPage();
    nav = new LinearLayout(this);
    nav.setPadding(dp(12), dp(4), dp(12), dp(8));
    String[] labels = {
      getString(R.string.tab_clock), getString(R.string.tab_alarm), getString(R.string.tab_settings)
    };
    for (int i = 0; i < 3; i++) {
      final int index = i;
      Button b =
          button(
              labels[i],
              () -> {
                tab = index;
                render();
              });
      b.setTextColor(i == tab ? 0xff29796e : 0xff6d7973);
      LinearLayout.LayoutParams p = new LinearLayout.LayoutParams(0, dp(56), 1);
      p.setMargins(dp(4), 0, dp(4), 0);
      nav.addView(b, p);
    }
    root.addView(nav);
  }

  private void clockPage() {
    body.addView(heading(getString(R.string.clock_page_title), 26));
    body.addView(text(getString(R.string.clock_tagline), 14, 0xff718079));
    body.addView(text(getString(R.string.clock_tip), 13, 0xff718079));
    clock = new ClockView(this);
    clock.setFace(
        Store.prefs(this).getBoolean("background", true),
        Store.prefs(this).getBoolean("ticks", true),
        Store.prefs(this).getBoolean("numbers", true));
    clock.setEffects(
        new ClockView.Effects() {
          @Override
          public void play(String name) {
            playCharacterSound(name);
          }

          @Override
          public void playAfter(String first, String next) {
            playCharacterSound(first, next);
          }
        });
    clock.setInteractionListener(() -> AlarmService.stopForClockInteraction(this));
    body.addView(clock, new LinearLayout.LayoutParams(-1, dp(300)));
    digital = heading("", 48);
    digital.setGravity(Gravity.CENTER);
    body.addView(digital);
    date = text("", 15, 0xff718079);
    date.setGravity(Gravity.CENTER);
    body.addView(date);
    tickNow();
    LinearLayout next = card();
    next.addView(heading(getString(R.string.upcoming_alarm), 15));
    long earliest = Long.MAX_VALUE;
    Alarm upcoming = null;
    for (Alarm a : Store.alarms(this)) {
      long time =
          a.enabled ? a.next(System.currentTimeMillis(), ZoneId.systemDefault()) : Long.MAX_VALUE;
      long snooze = Store.prefs(this).getLong("snooze_" + a.id, Long.MAX_VALUE);
      time = Math.min(time, snooze);
      if (time < earliest) {
        earliest = time;
        upcoming = a;
      }
    }
    next.addView(
        text(
            upcoming == null
                ? getString(R.string.no_upcoming_alarm)
                : Instant.ofEpochMilli(earliest)
                        .atZone(ZoneId.systemDefault())
                        .format(
                            DateTimeFormatter.ofPattern(
                                getString(R.string.next_alarm_date_format), LocaleHelper.locale(this)))
                    + " · "
                    + upcoming.label,
            16,
            0xff233b35));
    if (!Scheduler.allowed(this) && upcoming != null)
      next.addView(text(getString(R.string.exact_alarm_required), 14, 0xffad522e));
    next.addView(
        button(
            getString(R.string.manage_alarms),
            () -> {
              tab = 1;
              render();
            }));
    body.addView(button(getString(R.string.add_clock_widget), this::pinWidget));
  }

  private void tickNow() {
    if (digital != null) {
      ZonedDateTime now = ZonedDateTime.now();
      digital.setText(
          now.format(
              DateTimeFormatter.ofPattern(
                  Store.prefs(this).getBoolean("24hour", true) ? "HH:mm" : "a h:mm",
                  LocaleHelper.locale(this))));
      date.setText(
          now.format(
              DateTimeFormatter.ofPattern(
                  getString(R.string.date_format), LocaleHelper.locale(this))));
    }
  }

  private void playCharacterSound(String name) {
    playCharacterSound(name, null);
  }

  private void playCharacterSound(String name, String next) {
    int id = getResources().getIdentifier(name, "raw", getPackageName());
    if (id == 0) return;
    if (effect != null) {
      effect.release();
      effect = null;
    }
    effect = MediaPlayer.create(this, id);
    if (effect != null) {
      float volume = Store.prefs(this).getInt("effects", 80) / 100f;
      effect.setVolume(volume, volume);
      effect.setOnCompletionListener(
          player -> {
            player.release();
            if (effect == player) effect = null;
            if (next != null && resumed) playCharacterSound(next);
          });
      effect.start();
    }
  }

  private void alarmPage() {
    body.addView(heading(getString(R.string.alarm_page_title), 26));
    body.addView(text(getString(R.string.alarm_page_subtitle), 14, 0xff718079));
    permissions();
    body.addView(button(getString(R.string.add_alarm), () -> edit(null)));
    if (Store.alarms(this).isEmpty()) {
      LinearLayout empty = card();
      empty.addView(heading(getString(R.string.no_alarms), 21));
      empty.addView(text(getString(R.string.first_alarm_hint), 15, 0xff718079));
    }
    for (Alarm a : Store.alarms(this)) {
      LinearLayout c = card(), row = new LinearLayout(this);
      row.setGravity(Gravity.CENTER_VERTICAL);
      TextView time = heading(a.time(), 36);
      row.addView(time, new LinearLayout.LayoutParams(0, -2, 1));
      Switch toggle = new Switch(this);
      toggle.setContentDescription(getString(R.string.alarm_toggle_description, a.time()));
      toggle.setChecked(a.enabled);
      toggle.setOnCheckedChangeListener(
          (v, on) -> {
            a.enabled = on;
            Scheduler.cancel(this, a.id);
            Store.save(this, a);
            boolean scheduled = Scheduler.schedule(this, a);
            if (on && !scheduled) toast(getString(R.string.saved_alarm_permission));
          });
      row.addView(toggle);
      c.addView(row);
      c.addView(text(a.label + " · " + a.repeat(this), 15, 0xff718079));
      LinearLayout actions = new LinearLayout(this);
      LinearLayout.LayoutParams actionParams = new LinearLayout.LayoutParams(0, -2, 1);
      actionParams.setMargins(dp(4), dp(5), dp(4), dp(5));
      actions.addView(button(getString(R.string.edit), () -> edit(a)), actionParams);
      actionParams = new LinearLayout.LayoutParams(0, -2, 1);
      actionParams.setMargins(dp(4), dp(5), dp(4), dp(5));
      actions.addView(
          button(
              getString(R.string.delete),
              () ->
                  new AlertDialog.Builder(this)
                      .setTitle(R.string.delete_alarm_title)
                      .setMessage(getString(R.string.delete_alarm_message, a.time()))
                      .setNegativeButton(android.R.string.cancel, null)
                      .setPositiveButton(
                          getString(R.string.delete),
                          (d, w) -> {
                            Scheduler.cancel(this, a.id);
                            Store.delete(this, a.id);
                            render();
                          })
                      .show()),
          actionParams);
      c.addView(actions);
    }
  }

  private void edit(Alarm old) {
    LinearLayout form = new LinearLayout(this);
    form.setOrientation(LinearLayout.VERTICAL);
    form.setPadding(dp(18), dp(8), dp(18), dp(8));
    TimePicker picker =
        (TimePicker) getLayoutInflater().inflate(R.layout.alarm_time_picker, form, false);
    picker.setIs24HourView(Store.prefs(this).getBoolean("24hour", true));
    picker.setHour(old == null ? 7 : old.hour);
    picker.setMinute(old == null ? 0 : old.minute);
    form.addView(picker);
    EditText label = new EditText(this);
    label.setSingleLine(true);
    label.setHint(R.string.alarm_label_hint);
    label.setText(old == null ? getString(R.string.default_alarm_label) : old.label);
    form.addView(label);
    form.addView(text(getString(R.string.repeat_days_optional), 14, 0xff718079));
    String[] names = getResources().getStringArray(R.array.weekday_names);
    CheckBox[] days = new CheckBox[7];
    for (int start = 0; start < 7; start += 4) {
      LinearLayout row = new LinearLayout(this);
      for (int i = start; i < Math.min(start + 4, 7); i++) {
        days[i] = new CheckBox(this);
        days[i].setText(names[i]);
        days[i].setChecked(old != null && (old.days & (1 << i)) != 0);
        row.addView(days[i], new LinearLayout.LayoutParams(0, -2, 1));
      }
      form.addView(row);
    }
    ScrollView scroll = new ScrollView(this);
    scroll.addView(form);
    new AlertDialog.Builder(this)
        .setTitle(old == null ? R.string.new_alarm : R.string.edit_alarm)
        .setView(scroll)
        .setNegativeButton(android.R.string.cancel, null)
        .setPositiveButton(
            getString(R.string.save),
            (d, w) -> {
              int mask = 0;
              for (int i = 0; i < 7; i++) if (days[i].isChecked()) mask |= 1 << i;
              Alarm a =
                  new Alarm(
                      old == null ? Store.nextId(this) : old.id,
                      picker.getHour(),
                      picker.getMinute(),
                      mask,
                      old == null || old.enabled,
                      label.getText().toString().trim());
              Scheduler.cancel(this, a.id);
              Store.save(this, a);
              boolean ok = Scheduler.schedule(this, a);
              render();
              if (!ok) toast(getString(R.string.saved_alarm_needs_permission));
            })
        .show();
  }

  private void settingsPage() {
    body.addView(heading(getString(R.string.settings_page_title), 26));
    body.addView(text(getString(R.string.settings_page_subtitle), 14, 0xff718079));
    LinearLayout looks = card();
    looks.addView(heading(getString(R.string.appearance), 19));
    setting(looks, getString(R.string.show_background), "background", true);
    setting(looks, getString(R.string.show_ticks), "ticks", true);
    setting(looks, getString(R.string.show_numbers), "numbers", true);
    setting(looks, getString(R.string.twenty_four_hour), "24hour", true);
    LinearLayout audio = card();
    audio.addView(heading(getString(R.string.sound_vibration), 19));
    slider(audio, getString(R.string.alarm_volume), "volume", 80);
    audio.addView(text(getString(R.string.device_alarm_volume_note), 13, 0xff718079));
    slider(audio, getString(R.string.touch_volume), "effects", 80);
    setting(audio, getString(R.string.alarm_vibration), "vibration", true);
    audio.addView(
        text(
            Store.prefs(this).getString("sound", "").isEmpty()
                ? getString(R.string.alarm_sound_default)
                : getString(R.string.alarm_sound_custom),
            14,
            0xff718079));
    audio.addView(
        button(
            getString(R.string.choose_alarm_sound),
            () -> {
              Intent i =
                  new Intent(Intent.ACTION_OPEN_DOCUMENT)
                      .setType("audio/*")
                      .addCategory(Intent.CATEGORY_OPENABLE)
                      .addFlags(
                          Intent.FLAG_GRANT_READ_URI_PERMISSION
                              | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
              startActivityForResult(i, 20);
            }));
    audio.addView(
        button(
            getString(R.string.reset_alarm_sound),
            () -> {
              clearSound();
              render();
            }));
    permissions();

    // Language setting card
    {
      LinearLayout langCard = card();
      langCard.addView(heading(getString(R.string.language), 19));

      final String[] langEntries = {
        getString(R.string.language_auto),
        getString(R.string.language_ko),
        getString(R.string.language_ja),
        getString(R.string.language_en)
      };
      final String[] langValues = {"auto", "ko", "ja", "en"};
      String cur = LocaleHelper.selectedLanguage(this);
      int curIdx = 0;
      for (int i = 0; i < langValues.length; i++) if (langValues[i].equals(cur)) { curIdx = i; break; }
      final int selectedIndex = curIdx;

      Button langButton = button(getString(R.string.language) + ": " + langEntries[selectedIndex], () -> {
        new AlertDialog.Builder(this)
            .setTitle(getString(R.string.language))
            .setSingleChoiceItems(langEntries, selectedIndex, (d, which) -> {
              String sel = langValues[which];
              LocaleHelper.setLocale(this, sel);
              d.dismiss();
              recreate();
            })
            .setNegativeButton(android.R.string.cancel, null)
            .show();
      });
      langCard.addView(langButton);
    }

    body.addView(button(getString(R.string.add_clock_widget), this::pinWidget));
    body.addView(
        text(
            getString(R.string.settings_footer),
            13,
            0xff718079));
  }

  private void setting(LinearLayout parent, String title, String key, boolean fallback) {
    Switch s = new Switch(this);
    s.setText(title);
    s.setTextSize(16);
    s.setPadding(0, dp(12), 0, dp(12));
    s.setChecked(Store.prefs(this).getBoolean(key, fallback));
    s.setOnCheckedChangeListener(
        (v, on) -> {
          Store.prefs(this).edit().putBoolean(key, on).apply();
          ClockWidget.refresh(this);
        });
    parent.addView(s);
  }

  private void slider(LinearLayout parent, String name, String key, int fallback) {
    TextView t =
        text(
            getString(R.string.slider_value, name, Store.prefs(this).getInt(key, fallback)),
            15,
            0xff233b35);
    parent.addView(t);
    SeekBar s = new SeekBar(this);
    s.setContentDescription(name);
    s.setMax(100);
    s.setProgress(Store.prefs(this).getInt(key, fallback));
    s.setOnSeekBarChangeListener(
        new SeekBar.OnSeekBarChangeListener() {
          public void onProgressChanged(SeekBar b, int n, boolean user) {
            t.setText(getString(R.string.slider_value, name, n));
            if (user) Store.prefs(MainActivity.this).edit().putInt(key, n).apply();
          }

          public void onStartTrackingTouch(SeekBar b) {}

          public void onStopTrackingTouch(SeekBar b) {}
        });
    parent.addView(s);
  }

  private void permissions() {
    LinearLayout c = card();
    c.addView(heading(getString(R.string.alarm_status), 17));
    c.addView(
        text(
            Scheduler.allowed(this)
                ? getString(R.string.exact_alarm_available)
                : getString(R.string.exact_alarm_permission_needed),
            14,
            0xff29796e));
    if (!Scheduler.allowed(this))
      c.addView(
          button(
              getString(R.string.allow_exact_alarm),
              () ->
                  startActivity(
                      new Intent(
                          Settings.ACTION_REQUEST_SCHEDULE_EXACT_ALARM,
                          Uri.parse("package:" + getPackageName())))));
    NotificationManager nm = getSystemService(NotificationManager.class);
    if (!nm.areNotificationsEnabled())
      c.addView(
          button(
              getString(R.string.allow_alarm_notifications),
              () -> {
                if (Build.VERSION.SDK_INT >= 33
                    && checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS)
                        != PackageManager.PERMISSION_GRANTED
                    && (!Store.prefs(this).getBoolean("notificationRequested", false)
                        || shouldShowRequestPermissionRationale(
                            Manifest.permission.POST_NOTIFICATIONS))) {
                  Store.prefs(this).edit().putBoolean("notificationRequested", true).apply();
                  requestPermissions(new String[] {Manifest.permission.POST_NOTIFICATIONS}, 10);
                } else
                  startActivity(
                      new Intent(Settings.ACTION_APP_NOTIFICATION_SETTINGS)
                          .putExtra(Settings.EXTRA_APP_PACKAGE, getPackageName()));
              }));
    if (Build.VERSION.SDK_INT >= 34 && !nm.canUseFullScreenIntent())
      c.addView(
          button(
              getString(R.string.allow_lock_screen_alarm),
              () ->
                  startActivity(
                      new Intent(
                          Settings.ACTION_MANAGE_APP_USE_FULL_SCREEN_INTENT,
                          Uri.parse("package:" + getPackageName())))));
  }

  private void pinWidget() {
    AppWidgetManager m = AppWidgetManager.getInstance(this);
    if (m.isRequestPinAppWidgetSupported()) {
      m.requestPinAppWidget(new ComponentName(this, ClockWidget.class), null, null);
      playCharacterSound("answer");
    } else toast(getString(R.string.widget_manual_add_hint));
  }

  private void clearSound() {
    String old = Store.prefs(this).getString("sound", "");
    if (!old.isEmpty())
      try {
        getContentResolver()
            .releasePersistableUriPermission(Uri.parse(old), Intent.FLAG_GRANT_READ_URI_PERMISSION);
      } catch (SecurityException ignored) {
      }
    Store.prefs(this).edit().remove("sound").apply();
  }

  @Override
  protected void onActivityResult(int request, int result, Intent data) {
    super.onActivityResult(request, result, data);
    if (request == 20 && result == RESULT_OK && data != null && data.getData() != null) {
      Uri uri = data.getData();
      try {
        getContentResolver()
            .takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
        String old = Store.prefs(this).getString("sound", "");
        if (!old.equals(uri.toString())) clearSound();
        Store.prefs(this).edit().putString("sound", uri.toString()).apply();
        render();
      } catch (SecurityException e) {
        toast(getString(R.string.audio_file_unreadable));
      }
    }
  }

  @Override
  public void onRequestPermissionsResult(int r, String[] p, int[] results) {
    super.onRequestPermissionsResult(r, p, results);
    render();
  }

  private void toast(String message) {
    Toast.makeText(this, message, Toast.LENGTH_LONG).show();
  }
}
