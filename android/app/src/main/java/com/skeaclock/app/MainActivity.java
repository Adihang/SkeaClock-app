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
import androidx.preference.PreferenceManager;

public class MainActivity extends Activity {
  static final String WIDGET_BONK = "bonk";
  static final String WIDGET_CHEEK = "cheek";
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
                        java.util.Locale.KOREAN)));
            date.setText(
                now.format(DateTimeFormatter.ofPattern("M월 d일 EEEE", java.util.Locale.KOREAN)));
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
    String[] labels = {"시계", "알람", "설정"};
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
    body.addView(heading("스키아와 함께하는 시간", 26));
    body.addView(text("바보털은 오늘도 정확하게.", 14, 0xff718079));
    body.addView(text("가운데를 톡 눌러 꿀밤, 볼을 끌어당겨 반응을 확인해 보세요.", 13, 0xff718079));
    clock = new ClockView(this);
    clock.setFace(
        Store.prefs(this).getBoolean("background", true),
        Store.prefs(this).getBoolean("ticks", true),
        Store.prefs(this).getBoolean("numbers", true));
    clock.setEffects(this::playCharacterSound);
    body.addView(clock, new LinearLayout.LayoutParams(-1, dp(300)));
    digital = heading("", 48);
    digital.setGravity(Gravity.CENTER);
    body.addView(digital);
    date = text("", 15, 0xff718079);
    date.setGravity(Gravity.CENTER);
    body.addView(date);
    tickNow();
    LinearLayout next = card();
    next.addView(heading("다음 알람", 15));
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
                ? "여유로운 하루예요. 알람을 추가해 보세요."
                : Instant.ofEpochMilli(earliest)
                        .atZone(ZoneId.systemDefault())
                        .format(DateTimeFormatter.ofPattern("M월 d일 HH:mm"))
                    + " · "
                    + upcoming.label,
            16,
            0xff233b35));
    if (!Scheduler.allowed(this) && upcoming != null)
      next.addView(text("정확한 알람 권한을 허용해야 예약됩니다.", 14, 0xffad522e));
    next.addView(
        button(
            "알람 관리",
            () -> {
              tab = 1;
              render();
            }));
    body.addView(button("홈 화면에 시계 위젯 추가", this::pinWidget));
  }

  private void tickNow() {
    if (digital != null) {
      ZonedDateTime now = ZonedDateTime.now();
      digital.setText(
          now.format(
              DateTimeFormatter.ofPattern(
                  Store.prefs(this).getBoolean("24hour", true) ? "HH:mm" : "a h:mm",
                  java.util.Locale.KOREAN)));
      date.setText(now.format(DateTimeFormatter.ofPattern("M월 d일 EEEE", java.util.Locale.KOREAN)));
    }
  }

  private void playCharacterSound(String name) {
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
          });
      effect.start();
    }
  }

  private void alarmPage() {
    body.addView(heading("좋은 아침을 예약해요", 26));
    body.addView(text("시간과 반복 요일을 앱에서 관리하세요.", 14, 0xff718079));
    permissions();
    body.addView(button("+ 알람 추가", () -> edit(null)));
    if (Store.alarms(this).isEmpty()) {
      LinearLayout empty = card();
      empty.addView(heading("아직 알람이 없어요", 21));
      empty.addView(text("첫 알람을 추가하면 스키아가 깨워 드릴게요.", 15, 0xff718079));
    }
    for (Alarm a : Store.alarms(this)) {
      LinearLayout c = card(), row = new LinearLayout(this);
      row.setGravity(Gravity.CENTER_VERTICAL);
      TextView time = heading(a.time(), 36);
      row.addView(time, new LinearLayout.LayoutParams(0, -2, 1));
      Switch toggle = new Switch(this);
      toggle.setContentDescription(a.time() + " 알람 사용");
      toggle.setChecked(a.enabled);
      toggle.setOnCheckedChangeListener(
          (v, on) -> {
            a.enabled = on;
            Scheduler.cancel(this, a.id);
            Store.save(this, a);
            boolean scheduled = Scheduler.schedule(this, a);
            if (on && !scheduled) toast("알람은 저장됐습니다. 정확한 알람 권한을 허용해 주세요.");
          });
      row.addView(toggle);
      c.addView(row);
      c.addView(text(a.label + " · " + a.repeat(), 15, 0xff718079));
      LinearLayout actions = new LinearLayout(this);
      LinearLayout.LayoutParams actionParams = new LinearLayout.LayoutParams(0, -2, 1);
      actionParams.setMargins(dp(4), dp(5), dp(4), dp(5));
      actions.addView(button("편집", () -> edit(a)), actionParams);
      actionParams = new LinearLayout.LayoutParams(0, -2, 1);
      actionParams.setMargins(dp(4), dp(5), dp(4), dp(5));
      actions.addView(
          button(
              "삭제",
              () ->
                  new AlertDialog.Builder(this)
                      .setTitle("알람 삭제")
                      .setMessage(a.time() + " 알람을 삭제할까요?")
                      .setNegativeButton("취소", null)
                      .setPositiveButton(
                          "삭제",
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
    label.setHint("알람 이름");
    label.setText(old == null ? "좋은 아침" : old.label);
    form.addView(label);
    form.addView(text("반복 요일 · 선택하지 않으면 한 번", 14, 0xff718079));
    String[] names = {"월", "화", "수", "목", "금", "토", "일"};
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
        .setTitle(old == null ? "새 알람" : "알람 편집")
        .setView(scroll)
        .setNegativeButton("취소", null)
        .setPositiveButton(
            "저장",
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
              if (!ok) toast("저장 완료. 정확한 알람 권한을 허용하면 예약됩니다.");
            })
        .show();
  }

  private void settingsPage() {
    body.addView(heading("내 취향의 스키아", 26));
    body.addView(text("시계 모양은 홈 화면 위젯에도 반영돼요.", 14, 0xff718079));
    LinearLayout looks = card();
    looks.addView(heading("시계 모양", 19));
    setting(looks, "배경 표시", "background", true);
    setting(looks, "눈금 표시", "ticks", true);
    setting(looks, "숫자 표시", "numbers", true);
    setting(looks, "24시간 표시", "24hour", true);
    LinearLayout audio = card();
    audio.addView(heading("소리와 진동", 19));
    slider(audio, "알람 소리", "volume", 80);
    audio.addView(text("기기의 알람 음량도 함께 적용됩니다.", 13, 0xff718079));
    slider(audio, "터치 효과음", "effects", 80);
    setting(audio, "알람 진동", "vibration", true);
    audio.addView(
        text(
            Store.prefs(this).getString("sound", "").isEmpty()
                ? "알람음: 스키아 기본 음성"
                : "알람음: 선택한 오디오 파일",
            14,
            0xff718079));
    audio.addView(
        button(
            "알람 음성 파일 선택",
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
            "기본 알람음으로 되돌리기",
            () -> {
              clearSound();
              render();
            }));
    permissions();

    // Language setting card
    {
      LinearLayout langCard = card();
      langCard.addView(heading(getString(R.string.lang_title), 19));

      final String[] langEntries = new String[] {getString(R.string.lang_system), getString(R.string.lang_korean), getString(R.string.lang_english)};
      final String[] langValues = new String[] {"auto", "ko", "en"};

      android.content.SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(this);
      String cur = prefs.getString("selected_language", "auto");
      int curIdx = 0;
      for (int i = 0; i < langValues.length; i++) if (langValues[i].equals(cur)) { curIdx = i; break; }

      Button langButton = button(getString(R.string.settings_language) + ": " + langEntries[curIdx], () -> {
        new AlertDialog.Builder(this)
            .setTitle(getString(R.string.lang_title))
            .setSingleChoiceItems(langEntries, curIdx, (d, which) -> {
              String sel = langValues[which];
              if ("auto".equals(sel)) {
                LocaleHelper.setLocale(this, java.util.Locale.getDefault().getLanguage());
              } else {
                LocaleHelper.setLocale(this, sel);
              }
              d.dismiss();
              recreate();
            })
            .setNegativeButton(android.R.string.cancel, null)
            .show();
      });
      langCard.addView(langButton);
    }

    body.addView(button("홈 화면에 시계 위젯 추가", this::pinWidget));
    body.addView(
        text(
            "스키아 시계 1.0 · Android\n위젯은 시계만 표시하고, 투명한 입력 영역을 누르면 캐릭터가 바로 반응합니다. 알람은 최대 10분간 울립니다.",
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
    TextView t = text(name + "  " + Store.prefs(this).getInt(key, fallback) + "%", 15, 0xff233b35);
    parent.addView(t);
    SeekBar s = new SeekBar(this);
    s.setContentDescription(name);
    s.setMax(100);
    s.setProgress(Store.prefs(this).getInt(key, fallback));
    s.setOnSeekBarChangeListener(
        new SeekBar.OnSeekBarChangeListener() {
          public void onProgressChanged(SeekBar b, int n, boolean user) {
            t.setText(name + "  " + n + "%");
            if (user) Store.prefs(MainActivity.this).edit().putInt(key, n).apply();
          }

          public void onStartTrackingTouch(SeekBar b) {}

          public void onStopTrackingTouch(SeekBar b) {}
        });
    parent.addView(s);
  }

  private void permissions() {
    LinearLayout c = card();
    c.addView(heading("알람 준비 상태", 17));
    c.addView(
        text(Scheduler.allowed(this) ? "✓ 정확한 시간에 알람 예약 가능" : "정확한 알람 권한이 필요해요", 14, 0xff29796e));
    if (!Scheduler.allowed(this))
      c.addView(
          button(
              "정확한 알람 허용",
              () ->
                  startActivity(
                      new Intent(
                          Settings.ACTION_REQUEST_SCHEDULE_EXACT_ALARM,
                          Uri.parse("package:" + getPackageName())))));
    NotificationManager nm = getSystemService(NotificationManager.class);
    if (!nm.areNotificationsEnabled())
      c.addView(
          button(
              "알람 알림 허용",
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
              "잠금 화면 알람 허용",
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
    } else toast("홈 화면을 길게 누른 뒤 위젯 → 스키아 시계를 선택하세요.");
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
        toast("파일을 읽을 수 없습니다. 다른 오디오 파일을 선택해 주세요.");
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
