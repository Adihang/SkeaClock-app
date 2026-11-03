package com.skeaclock.app;

import android.app.PendingIntent;
import android.appwidget.*;
import android.content.*;
import android.widget.RemoteViews;

public class ClockWidget extends AppWidgetProvider {
  private static final int INPUT_GRID_SIZE = 5;
  private static final int BONK_TOP_ROW = 1;
  private static final int BONK_BOTTOM_ROW = 2;
  private static final int BONK_LEFT_COLUMN = 1;
  private static final int BONK_RIGHT_COLUMN = 3;
  private static final int PAT_ROW = 3;
  private static final int PAT_COLUMN = 1;
  private static final float BONK_HEAD_X = 240f;
  private static final float BONK_HEAD_Y = 180f;
  private static final int[] CONTROLS = {
    R.id.widget_r0c0, R.id.widget_r0c1, R.id.widget_r0c2, R.id.widget_r0c3, R.id.widget_r0c4,
    R.id.widget_r1c0, R.id.widget_bonk_head_0, R.id.widget_bonk_head_1, R.id.widget_bonk_head_2, R.id.widget_r1c4,
    R.id.widget_r2c0, R.id.widget_bonk_head_3, R.id.widget_bonk_head_4, R.id.widget_bonk_head_5, R.id.widget_r2c4,
    R.id.widget_r3c0, R.id.widget_r3c1, R.id.widget_r3c2, R.id.widget_r3c3, R.id.widget_r3c4,
    R.id.widget_r4c0, R.id.widget_r4c1, R.id.widget_r4c2, R.id.widget_r4c3, R.id.widget_r4c4
  };

  @Override
  public void onUpdate(Context c, AppWidgetManager manager, int[] ids) {
    Context localized = LocaleHelper.onAttach(c);
    for (int id : ids) {
      RemoteViews v = new RemoteViews(c.getPackageName(), R.layout.widget_clock);
      v.setImageViewResource(R.id.dial, Store.dialBackground(c));
      v.setImageViewResource(R.id.widget_idle_character, R.drawable.widget_idle);
      v.setImageViewResource(R.id.dial_markers, Store.dialMarkers(c));
      for (int i = 0; i < CONTROLS.length; i++) {
        int row = i / INPUT_GRID_SIZE;
        int column = i % INPUT_GRID_SIZE;
        int dx = column - INPUT_GRID_SIZE / 2;
        int dy = row - INPUT_GRID_SIZE / 2;
        boolean bonk =
            row >= BONK_TOP_ROW
                && row <= BONK_BOTTOM_ROW
                && column >= BONK_LEFT_COLUMN
                && column <= BONK_RIGHT_COLUMN;
        boolean pat = row == PAT_ROW && column == PAT_COLUMN;
        if (!bonk && dx == 0 && dy == 0) dy = 1;
        String description =
            bonk
                ? localized.getString(R.string.widget_bonk_accessibility)
                : pat
                    ? localized.getString(R.string.widget_pat_accessibility)
                    : describeCheekPull(localized, dx, dy);
        v.setContentDescription(CONTROLS[i], description);
        v.setOnClickPendingIntent(
            CONTROLS[i],
            widgetAction(
                c,
                id,
                10 + i,
                bonk
                    ? MainActivity.WIDGET_BONK
                    : pat ? MainActivity.WIDGET_PAT : MainActivity.WIDGET_CHEEK,
                bonk ? BONK_HEAD_X : dx,
                bonk ? BONK_HEAD_Y : dy));
      }
      manager.updateAppWidget(id, v);
    }
  }

  private static String describeCheekPull(Context context, int dx, int dy) {
    int direction =
        Math.floorMod(
            (int) Math.round(Math.atan2(dy, dx) / (Math.PI / 8)),
            context.getResources().getStringArray(R.array.cheek_directions).length);
    String[] directions = context.getResources().getStringArray(R.array.cheek_directions);
    return context.getString(R.string.widget_cheek_accessibility, directions[direction]);
  }

  private PendingIntent widgetAction(
      Context context,
      int widgetId,
      int cellRequestCode,
      String action,
      float actionX,
      float actionY) {
    Intent intent =
        new Intent(context, WidgetActionReceiver.class)
            .setAction(WidgetActionReceiver.ACTION_WIDGET_INTERACTION)
            .putExtra(WidgetActionReceiver.EXTRA_WIDGET_ID, widgetId)
            .putExtra(WidgetActionReceiver.EXTRA_ACTION, action)
            .putExtra(WidgetActionReceiver.EXTRA_X, actionX)
            .putExtra(WidgetActionReceiver.EXTRA_Y, actionY);
    return PendingIntent.getBroadcast(
        context,
        pendingIntentRequestCode(widgetId * 32 + cellRequestCode),
        intent,
        PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
  }

  protected int pendingIntentRequestCode(int requestCode) {
    return requestCode;
  }

  static void refresh(Context c) {
    AppWidgetManager m = AppWidgetManager.getInstance(c);
    new ClockWidget().onUpdate(c, m, m.getAppWidgetIds(new ComponentName(c, ClockWidget.class)));
    new CoverClockWidget()
        .onUpdate(c, m, m.getAppWidgetIds(new ComponentName(c, CoverClockWidget.class)));
  }
}
