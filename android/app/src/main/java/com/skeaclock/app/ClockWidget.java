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
  private static final float BONK_HEAD_X = 240f;
  private static final float BONK_HEAD_Y = 180f;
  private static final String[] INPUT_DIRECTIONS = {
    "오른쪽",
    "오른쪽에서 아래쪽으로 22.5도",
    "오른쪽 아래",
    "아래쪽에서 오른쪽으로 22.5도",
    "아래쪽",
    "아래쪽에서 왼쪽으로 22.5도",
    "왼쪽 아래",
    "왼쪽에서 아래쪽으로 22.5도",
    "왼쪽",
    "왼쪽에서 위쪽으로 22.5도",
    "왼쪽 위",
    "위쪽에서 왼쪽으로 22.5도",
    "위쪽",
    "위쪽에서 오른쪽으로 22.5도",
    "오른쪽 위",
    "오른쪽에서 위쪽으로 22.5도"
  };
  private static final int[] CONTROLS = {
    R.id.widget_r0c0, R.id.widget_r0c1, R.id.widget_r0c2, R.id.widget_r0c3, R.id.widget_r0c4,
    R.id.widget_r1c0, R.id.widget_bonk_head_0, R.id.widget_bonk_head_1, R.id.widget_bonk_head_2, R.id.widget_r1c4,
    R.id.widget_r2c0, R.id.widget_bonk_head_3, R.id.widget_bonk_head_4, R.id.widget_bonk_head_5, R.id.widget_r2c4,
    R.id.widget_r3c0, R.id.widget_r3c1, R.id.widget_r3c2, R.id.widget_r3c3, R.id.widget_r3c4,
    R.id.widget_r4c0, R.id.widget_r4c1, R.id.widget_r4c2, R.id.widget_r4c3, R.id.widget_r4c4
  };

  @Override
  public void onUpdate(Context c, AppWidgetManager manager, int[] ids) {
    for (int id : ids) {
      RemoteViews v = new RemoteViews(c.getPackageName(), R.layout.widget_clock);
      v.setImageViewResource(R.id.dial, Store.dial(c));
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
        if (!bonk && dx == 0 && dy == 0) dy = 1;
        String description =
            bonk ? "머리 영역 꿀밤 애니메이션 실행" : describeCheekPull(dx, dy);
        v.setContentDescription(CONTROLS[i], description);
        v.setOnClickPendingIntent(
            CONTROLS[i],
            widgetAction(
                c,
                id,
                10 + i,
                bonk ? MainActivity.WIDGET_BONK : MainActivity.WIDGET_CHEEK,
                bonk ? BONK_HEAD_X : dx,
                bonk ? BONK_HEAD_Y : dy));
      }
      manager.updateAppWidget(id, v);
    }
  }

  private static String describeCheekPull(int dx, int dy) {
    int direction =
        Math.floorMod((int) Math.round(Math.atan2(dy, dx) / (Math.PI / 8)), INPUT_DIRECTIONS.length);
    return INPUT_DIRECTIONS[direction] + " 방향으로 볼 당기기";
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
