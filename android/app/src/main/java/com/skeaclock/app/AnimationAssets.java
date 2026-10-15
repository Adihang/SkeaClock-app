package com.skeaclock.app;

import android.content.Context;
import android.graphics.Movie;
import java.io.InputStream;
import java.util.HashMap;
import java.util.Map;
import org.json.JSONArray;
import org.json.JSONObject;

/** Original frame timings and alignment metadata extracted from the bundled Windows artwork. */
final class AnimationAssets {
  private static final String[] ALL_CLIPS = {
    "skea_idle",
    "skea_transition",
    "skea_touch_back",
    "skea_touch_head",
    "skea_touch_front",
    "skea_touch_end",
    "skea_touch_end_back",
    "skea_touch_end_head",
    "skea_touch_end_front",
    "skea_smash",
    "skea_angry",
    "skea_angry_alt",
    "skea_panic",
    "skea_blank"
  };

  static final class Clip {
    final Movie movie;
    final int[] ends;
    final int width, height;
    final float offsetX, offsetY, pointX, pointY, radius;
    final float[][] landmarks, centers;

    Clip(Movie movie, JSONObject json) throws Exception {
      this.movie = movie;
      width = json.getInt("width");
      height = json.getInt("height");
      JSONArray durations = json.getJSONArray("durations");
      ends = new int[durations.length()];
      int total = 0;
      for (int i = 0; i < ends.length; i++) ends[i] = total += durations.getInt(i);
      offsetX = (float) json.optDouble("offsetX", 0);
      offsetY = (float) json.optDouble("offsetY", 0);
      pointX = (float) json.optDouble("x", 0);
      pointY = (float) json.optDouble("y", 0);
      radius = (float) json.optDouble("radius", 16);
      landmarks = points(json.optJSONArray("landmarks"));
      centers = points(json.optJSONArray("centers"));
    }

    private static float[][] points(JSONArray values) throws Exception {
      float[][] result = new float[values == null ? 0 : values.length()][2];
      if (values != null)
        for (int i = 0; i < result.length; i++) {
          result[i][0] = (float) values.getJSONArray(i).getDouble(0);
          result[i][1] = (float) values.getJSONArray(i).getDouble(1);
        }
      return result;
    }

    int duration() {
      return ends[ends.length - 1];
    }

    int frameAt(long time) {
      int position = (int) Math.floorMod(time, (long) duration());
      int low = 0, high = ends.length;
      while (low < high) {
        int mid = (low + high) >>> 1;
        if (ends[mid] <= position) low = mid + 1;
        else high = mid;
      }
      return Math.min(low, ends.length - 1);
    }

    int frameStart(int frame) {
      return frame <= 0 ? 0 : ends[Math.min(frame, ends.length) - 1];
    }

    int frameEnd(int frame) {
      return ends[Math.min(frame, ends.length - 1)];
    }
  }

  final Map<String, Clip> clips = new HashMap<>();

  private AnimationAssets() {}

  static AnimationAssets load(Context context) {
    return load(context, ALL_CLIPS);
  }

  static AnimationAssets load(Context context, String... names) {
    AnimationAssets result = new AnimationAssets();
    try (InputStream input = context.getAssets().open("skea_animation.json")) {
      byte[] bytes = new byte[input.available()];
      int offset = 0;
      while (offset < bytes.length) offset += input.read(bytes, offset, bytes.length - offset);
      JSONObject root = new JSONObject(new String(bytes, java.nio.charset.StandardCharsets.UTF_8));
      JSONObject definitions = root.getJSONObject("animations");
      for (String name : names) {
        JSONObject json = definitions.getJSONObject(name);
        int id = context.getResources().getIdentifier(name, "raw", context.getPackageName());
        try (InputStream gif = context.getResources().openRawResource(id)) {
          Movie movie = Movie.decodeStream(gif);
          if (movie == null) throw new IllegalStateException("Invalid GIF: " + name);
          result.clips.put(name, new Clip(movie, json));
        }
      }
      if (result.clips.size() != names.length)
        throw new IllegalStateException("Missing character animation");
      return result;
    } catch (Exception e) {
      throw new IllegalStateException("Could not load Skea animations", e);
    }
  }
}
