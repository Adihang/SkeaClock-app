package com.skeaclock.app;

import android.content.*;
import java.util.*;
import org.json.*;

final class Store {
  static SharedPreferences prefs(Context c) {
    return c.getSharedPreferences("skea", Context.MODE_PRIVATE);
  }

  static java.util.List<Alarm> alarms(Context c) {
    List<Alarm> result = new ArrayList<>();
    try {
      JSONArray a = new JSONArray(prefs(c).getString("alarms", "[]"));
      for (int i = 0; i < a.length(); i++) result.add(Alarm.from(a.getJSONObject(i)));
    } catch (JSONException e) {
      android.util.Log.e("SkeaClock", "Invalid saved alarms", e);
    }
    return result;
  }

  static Alarm find(Context c, int id) {
    for (Alarm a : alarms(c)) if (a.id == id) return a;
    return null;
  }

  static void save(Context c, Alarm alarm) {
    List<Alarm> list = alarms(c);
    list.removeIf(a -> a.id == alarm.id);
    list.add(alarm);
    list.sort(Comparator.comparingInt(a -> a.hour * 60 + a.minute));
    write(c, list);
  }

  static void delete(Context c, int id) {
    List<Alarm> list = alarms(c);
    list.removeIf(a -> a.id == id);
    write(c, list);
  }

  private static void write(Context c, List<Alarm> list) {
    JSONArray j = new JSONArray();
    for (Alarm a : list) j.put(a.json());
    prefs(c).edit().putString("alarms", j.toString()).commit();
  }

  static int nextId(Context c) {
    int id = prefs(c).getInt("nextId", 1);
    prefs(c).edit().putInt("nextId", id + 1).commit();
    return id;
  }

  static int dial(Context c) {
    int flags =
        (prefs(c).getBoolean("background", true) ? 1 : 0)
            | (prefs(c).getBoolean("ticks", true) ? 2 : 0)
            | (prefs(c).getBoolean("numbers", true) ? 4 : 0);
    return new int[] {
          R.drawable.dial_0,
          R.drawable.dial_1,
          R.drawable.dial_2,
          R.drawable.dial_3,
          R.drawable.dial_4,
          R.drawable.dial_5,
          R.drawable.dial_6,
          R.drawable.dial_7
        }
        [flags];
  }
}
