package com.skeaclock.app;

import android.app.*;
import android.content.*;
import android.os.*;
import android.view.*;
import android.widget.*;

public class RingActivity extends Activity {
  private android.media.MediaPlayer alert;
  private final BroadcastReceiver closed =
      new BroadcastReceiver() {
        @Override
        public void onReceive(Context c, Intent i) {
          finish();
        }
      };

  @android.annotation.SuppressLint(
      "UnspecifiedRegisterReceiverFlag") // API <33 has no export flag; broadcast is package-scoped.
  @Override
  public void onCreate(Bundle state) {
    super.onCreate(state);
    getWindow()
        .addFlags(
            WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON
                | WindowManager.LayoutParams.FLAG_SHOW_WHEN_LOCKED
                | WindowManager.LayoutParams.FLAG_TURN_SCREEN_ON);
    if (Build.VERSION.SDK_INT >= 33)
      registerReceiver(
          closed, new IntentFilter(AlarmService.CLOSED), Context.RECEIVER_NOT_EXPORTED);
    else registerReceiver(closed, new IntentFilter(AlarmService.CLOSED));
    render();
  }

  @Override
  protected void onNewIntent(Intent i) {
    super.onNewIntent(i);
    setIntent(i);
    render();
  }

  private void render() {
    LinearLayout box = new LinearLayout(this);
    box.setOrientation(LinearLayout.VERTICAL);
    box.setGravity(Gravity.CENTER);
    box.setPadding(32, 64, 32, 64);
    box.setBackgroundColor(0xfff7f9f5);
    Alarm a = Store.find(this, getIntent().getIntExtra("id", -1));
    TextView title = new TextView(this);
    title.setText(a == null ? "일어날 시간이에요" : a.label + "\n" + a.time());
    title.setTextSize(32);
    title.setGravity(Gravity.CENTER);
    box.addView(title);
    box.addView(new ClockView(this), new LinearLayout.LayoutParams(-1, 0, 1));
    Button snooze = new Button(this);
    snooze.setText("5분 후 다시 울리기");
    snooze.setOnClickListener(v -> command("SNOOZE"));
    box.addView(snooze, new LinearLayout.LayoutParams(-1, 64));
    Button stop = new Button(this);
    stop.setText("알람 끄기");
    stop.setOnClickListener(v -> command("STOP"));
    box.addView(stop, new LinearLayout.LayoutParams(-1, 64));
    setContentView(box);
  }

  private void command(String action) {
    startService(new Intent(this, AlarmService.class).setAction(action));
  }

  @Override
  protected void onResume() {
    super.onResume();
    if (AlarmService.activeId == -1) finish();
  }

  @Override
  protected void onDestroy() {
    if (alert != null) {
      alert.release();
      alert = null;
    }
    unregisterReceiver(closed);
    super.onDestroy();
  }
}
