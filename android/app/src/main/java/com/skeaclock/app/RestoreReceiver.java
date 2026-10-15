package com.skeaclock.app;

import android.content.*;

public class RestoreReceiver extends BroadcastReceiver {
  @Override
  public void onReceive(Context c, Intent i) {
    Scheduler.restore(c);
    ClockWidget.refresh(c);
  }
}
