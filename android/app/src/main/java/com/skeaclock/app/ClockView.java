package com.skeaclock.app;

import android.content.Context;
import android.graphics.*;
import android.os.SystemClock;
import android.view.MotionEvent;
import android.view.View;
import java.time.ZonedDateTime;
import java.util.Random;

/** Original character animations, cheek deformation, hand motion, and clock face renderer. */
public class ClockView extends View {
  interface Effects {
    void play(String name);
  }

  private enum Mode {
    IDLE,
    SMASH,
    TOUCH,
    RELEASE,
    PANIC
  }

  private static final int IDLE_RETURN_FRAME = 71;
  private static final int SMASH_START_FRAME = 4;
  private static final int ANGRY_CUT_FRAME = 14;
  private static final int ANGRY_ALT_START_FRAME = 23;
  private static final int TRANSITION_START_FRAME = 86;
  private static final int TRANSITION_END_FRAME = 109;
  private static final int TOUCH_BOUNCE_FRAMES = 6;
  private static final long ALARM_HAND_GATHER_MS = 100;
  private static final long CHEEK_PULL_RELEASE_MS = 480;
  private static final float CHARACTER_SCALE = 1.6f;
  private static final float DIAL_RADIUS = 216f;
  private static final int ROWS = 24, COLS = 24, STAR_COUNT = 6;
  private static final int HEAD_PADDING = 40;

  private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.FILTER_BITMAP_FLAG);
  private final Paint line = new Paint(Paint.ANTI_ALIAS_FLAG);
  private final Paint star = new Paint(Paint.ANTI_ALIAS_FLAG);
  private final AnimationAssets assets;
  private final AnimationAssets.Clip idle,
      transition,
      touchBack,
      touchHead,
      touchFront,
      touchEnd,
      touchEndBack,
      touchEndHead,
      touchEndFront,
      smash,
      angry,
      angryAlt,
      panic;
  private final Bitmap hourHand, minuteHand;
  private final Bitmap touchComposite, releaseComposite, touchHeadBitmap, releaseHeadBitmap;
  private final Canvas touchCompositeCanvas, releaseCompositeCanvas, touchHeadCanvas, releaseHeadCanvas;
  private final float[] mesh = new float[(ROWS + 1) * (COLS + 1) * 2];
  private final float[] starX = new float[STAR_COUNT], starY = new float[STAR_COUNT];
  private final float[] starVx = new float[STAR_COUNT], starVy = new float[STAR_COUNT];
  private final float[] starGravity = new float[STAR_COUNT], starSize = new float[STAR_COUNT];
  private final float[] starDelay = new float[STAR_COUNT], starRotation = new float[STAR_COUNT];
  private long stateStarted = SystemClock.uptimeMillis(), idleOffset, handReturnStarted, smashAt;
  private long idleSoundAt = Long.MAX_VALUE;
  private float pullX, pullY, releasePullX, releasePullY, lastTouchX, lastTouchY;
  private int releaseLandmarkFrame;
  private float handReturnFromHour = .5f, handReturnFromMinute = .5f;
  private boolean handReturn,
      transitionHandsStarted,
      showBackground = true,
      showTicks = true,
      showNumbers = true;
  private boolean alarmView, alarmAlertPlayed;
  private Mode mode = Mode.IDLE;
  private Effects effects;
  private final Random random = new Random();

  public ClockView(Context context) {
    this(context, false);
  }

  public ClockView(Context context, boolean ringing) {
    this(context, ringing, null);
  }

  ClockView(Context context, String widgetAction) {
    this(context, false, widgetAnimationClips(widgetAction));
  }

  private ClockView(Context context, boolean ringing, String[] requestedClips) {
    super(context);
    assets =
        requestedClips == null
            ? AnimationAssets.load(context)
            : AnimationAssets.load(context, requestedClips);
    idle = clip("skea_idle");
    transition = clip("skea_transition");
    touchBack = clip("skea_touch_back");
    touchHead = clip("skea_touch_head");
    touchFront = clip("skea_touch_front");
    touchEnd = clip("skea_touch_end");
    touchEndBack = clip("skea_touch_end_back");
    touchEndHead = clip("skea_touch_end_head");
    touchEndFront = clip("skea_touch_end_front");
    smash = clip("skea_smash");
    angry = clip("skea_angry");
    angryAlt = clip("skea_angry_alt");
    panic = clip("skea_panic");
    hourHand = decode(R.drawable.hour_hand);
    minuteHand = decode(R.drawable.minute_hand);
    if (touchBack != null) {
      touchComposite =
          Bitmap.createBitmap(touchBack.width, touchBack.height, Bitmap.Config.ARGB_8888);
      releaseComposite =
          Bitmap.createBitmap(touchEndBack.width, touchEndBack.height, Bitmap.Config.ARGB_8888);
      touchHeadBitmap =
          Bitmap.createBitmap(
              touchHead.width + HEAD_PADDING * 2,
              touchHead.height + HEAD_PADDING * 2,
              Bitmap.Config.ARGB_8888);
      releaseHeadBitmap =
          Bitmap.createBitmap(
              touchEndHead.width + HEAD_PADDING * 2,
              touchEndHead.height + HEAD_PADDING * 2,
              Bitmap.Config.ARGB_8888);
      touchCompositeCanvas = new Canvas(touchComposite);
      releaseCompositeCanvas = new Canvas(releaseComposite);
      touchHeadCanvas = new Canvas(touchHeadBitmap);
      releaseHeadCanvas = new Canvas(releaseHeadBitmap);
    } else {
      touchComposite = releaseComposite = touchHeadBitmap = releaseHeadBitmap = null;
      touchCompositeCanvas = releaseCompositeCanvas = touchHeadCanvas = releaseHeadCanvas = null;
    }
    setLayerType(View.LAYER_TYPE_SOFTWARE, null);
    setContentDescription("스키아 시계. 가운데를 눌러 반응을 보고 볼을 끌어당기세요.");
    setClickable(true);
    if (ringing) {
      mode = Mode.PANIC;
      stateStarted = SystemClock.uptimeMillis();
      alarmView = true;
      alarmAlertPlayed = false;
    }
  }

  private static String[] widgetAnimationClips(String action) {
    if (MainActivity.WIDGET_BONK.equals(action))
      return
          new String[] {
            "skea_idle", "skea_transition", "skea_smash", "skea_angry", "skea_angry_alt"
          };
    return
        new String[] {
          "skea_idle",
          "skea_transition",
          "skea_touch_back",
          "skea_touch_head",
          "skea_touch_front",
          "skea_touch_end",
          "skea_touch_end_back",
          "skea_touch_end_head",
          "skea_touch_end_front"
        };
  }

  private AnimationAssets.Clip clip(String name) {
    return assets.clips.get(name);
  }

  private Bitmap decode(int id) {
    BitmapFactory.Options options = new BitmapFactory.Options();
    options.inScaled = false;
    return BitmapFactory.decodeResource(getResources(), id, options);
  }

  void setEffects(Effects effects) {
    this.effects = effects;
  }

  void setAlarmRinging(boolean ringing) {
    if (ringing) {
      mode = Mode.PANIC;
      stateStarted = SystemClock.uptimeMillis();
      handReturn = false;
      alarmAlertPlayed = false;
    } else if (mode == Mode.PANIC) returnToIdle(true);
    invalidate();
  }

  void setFace(boolean background, boolean ticks, boolean numbers) {
    showBackground = background;
    showTicks = ticks;
    showNumbers = numbers;
    invalidate();
  }

  String modeName() {
    return mode.name();
  }

  void triggerSmash() {
    startSmash(240, 240);
  }

  void triggerSmash(float hitX, float hitY) {
    startSmash(hitX, hitY);
  }

  void triggerCheekPull() {
    triggerCheekPull(-1, 0);
  }

  void triggerCheekPull(float directionX, float directionY) {
    triggerCheekPull(directionX, directionY, true);
  }

  void triggerWidgetCheekPull(float directionX, float directionY) {
    triggerCheekPull(directionX, directionY, false);
  }

  void finishWidgetCheekPull() {
    if (mode == Mode.TOUCH) finishTouch();
  }

  private void triggerCheekPull(float directionX, float directionY, boolean scheduleRelease) {
    if (mode == Mode.PANIC) return;
    float directionLength = (float) Math.hypot(directionX, directionY);
    if (directionLength < .01f) {
      directionX = -1;
      directionY = 0;
    } else {
      directionX /= directionLength;
      directionY /= directionLength;
    }
    float x = 240 + (transition.pointX - idle.centers[0][0]) * CHARACTER_SCALE;
    float y = 240 + (transition.pointY - idle.centers[0][1]) * CHARACTER_SCALE;
    startTouch(x, y);
    pullX = directionX * touchHead.radius * 1.4f;
    pullY = directionY * touchHead.radius * 1.4f;
    long started = stateStarted;
    postInvalidateOnAnimation();
    if (scheduleRelease)
      postDelayed(() -> {
        if (mode == Mode.TOUCH && stateStarted == started) finishTouch();
      }, CHEEK_PULL_RELEASE_MS);
  }

  private void startSmash(float x, float y) {
    if (mode == Mode.PANIC) return;
    mode = Mode.SMASH;
    transitionHandsStarted = false;
    stateStarted = SystemClock.uptimeMillis();
    idleOffset = 0;
    handReturn = false;
    beginStars(x, y);
    if (getParent() != null) getParent().requestDisallowInterceptTouchEvent(true);
    if (effects != null) effects.play("smash");
    invalidate();
  }

  private void startTouch(float x, float y) {
    mode = Mode.TOUCH;
    stateStarted = SystemClock.uptimeMillis();
    idleSoundAt = stateStarted + 600;
    pullX = pullY = 0;
    lastTouchX = x;
    lastTouchY = y;
    if (getParent() != null) getParent().requestDisallowInterceptTouchEvent(true);
    if (effects != null) effects.play("touch_start");
    invalidate();
  }

  private void finishTouch() {
    releaseLandmarkFrame = touchBack.frameAt(SystemClock.uptimeMillis() - stateStarted);
    mode = Mode.RELEASE;
    stateStarted = SystemClock.uptimeMillis();
    releasePullX = pullX;
    releasePullY = pullY;
    pullX = pullY = 0;
    if (getParent() != null) getParent().requestDisallowInterceptTouchEvent(false);
    if (effects != null) effects.play("touch_end");
    invalidate();
  }

  @Override
  protected void onDraw(Canvas canvas) {
    super.onDraw(canvas);
    final long now = SystemClock.uptimeMillis(), elapsed = Math.max(0, now - stateStarted);
    float size = Math.min(getWidth(), getHeight());
    if (size <= 0) return;
    canvas.save();
    canvas.translate((getWidth() - size) / 2f, (getHeight() - size) / 2f);
    canvas.scale(size / 480f, size / 480f);
    drawClock(canvas, now, elapsed);
    canvas.restore();
    maybePlayIdle(now);
    postInvalidateDelayed(nextDelay(now));
  }

  void drawWidgetFrame(Canvas canvas) {
    long now = SystemClock.uptimeMillis(), elapsed = Math.max(0, now - stateStarted);
    float size = Math.min(getWidth(), getHeight());
    if (size <= 0) return;
    canvas.save();
    canvas.translate((getWidth() - size) / 2f, (getHeight() - size) / 2f);
    canvas.scale(size / 480f, size / 480f);
    drawClock(canvas, now, elapsed);
    canvas.restore();
    maybePlayIdle(now);
  }

  private void drawClock(Canvas c, long now, long elapsed) {
    if (showBackground) {
      paint.setColor(Color.rgb(0xdf, 0xf7, 0xf5));
      paint.setStyle(Paint.Style.FILL);
      c.drawCircle(240, 240, DIAL_RADIUS, paint);
    }
    c.save();
    Path dial = new Path();
    dial.addCircle(240, 240, DIAL_RADIUS, Path.Direction.CW);
    c.clipPath(dial);
    drawCharacter(c, now, elapsed);
    if (mode != Mode.PANIC
        && mode != Mode.TOUCH
        && mode != Mode.RELEASE
        && !(mode == Mode.SMASH && reactionStage(elapsed) != 3)) drawHands(c, now, elapsed);
    c.restore();
    drawMarkers(c);
    drawStars(c, now);
  }

  private int reactionStage(long elapsed) {
    long smashOffset = smash.frameStart(SMASH_START_FRAME);
    long smashActive = smash.duration() - smashOffset;
    long angryStart = smashActive;
    long angryAltStart = angryStart + angry.duration() * 2L + angry.frameStart(ANGRY_CUT_FRAME);
    long angryAltLength = adjustedAltDuration();
    long transitionStart = angryAltStart + angryAltLength;
    long transitionEnd =
        transitionStart
            + Math.round(
                (transition.frameEnd(TRANSITION_END_FRAME)
                        - transition.frameStart(TRANSITION_START_FRAME))
                    / .75f);
    if (elapsed < angryStart) return 0;
    if (elapsed < angryAltStart) return 1;
    if (elapsed < transitionStart) return 2;
    if (elapsed < transitionEnd) return 3;
    return 4;
  }

  private long adjustedAltDuration() {
    long total = 0;
    for (int f = ANGRY_ALT_START_FRAME; f < angryAlt.ends.length; f++) {
      float progress =
          (float) (f - ANGRY_ALT_START_FRAME) / (angryAlt.ends.length - 1 - ANGRY_ALT_START_FRAME);
      float speed = 1 - progress * .5f;
      total += Math.round((angryAlt.frameEnd(f) - angryAlt.frameStart(f)) / speed);
    }
    return total;
  }

  private int adjustedAltSourceTime(long elapsed) {
    long active = 0;
    for (int f = ANGRY_ALT_START_FRAME; f < angryAlt.ends.length; f++) {
      float progress =
          (float) (f - ANGRY_ALT_START_FRAME) / (angryAlt.ends.length - 1 - ANGRY_ALT_START_FRAME);
      float speed = 1 - progress * .5f;
      int sourceDelay = angryAlt.frameEnd(f) - angryAlt.frameStart(f);
      long played = Math.round(sourceDelay / speed);
      if (elapsed < active + played)
        return angryAlt.frameStart(f) + (int) ((elapsed - active) * speed);
      active += played;
    }
    return angryAlt.duration() - 1;
  }

  private void drawCharacter(Canvas c, long now, long elapsed) {
    switch (mode) {
      case IDLE:
        drawMovie(c, idle, idleOffset + elapsed, 0, 0);
        break;
      case SMASH:
        int stage = reactionStage(elapsed);
        if (stage == 0) drawMovie(c, smash, smash.frameStart(SMASH_START_FRAME) + elapsed, 1, -15);
        else if (stage == 1) drawMovie(c, angry, elapsed - reactionStartAngry(), 59, -35);
        else if (stage == 2) {
          long altElapsed = elapsed - reactionStartAlt();
          if (altElapsed < 0) altElapsed = 0;
          int source = adjustedAltSourceTime(altElapsed);
          drawMovie(c, angryAlt, source, 59, -35);
        } else if (stage == 3) {
          long local = elapsed - reactionStartTransition();
          drawMovie(
              c,
              transition,
              transition.frameStart(TRANSITION_START_FRAME) + Math.round(local * .75f),
              0,
              0);
        } else {
          mode = Mode.IDLE;
          stateStarted = now;
          idleOffset = idle.frameStart(IDLE_RETURN_FRAME - 1);
          handReturn = true;
          handReturnStarted = now;
          handReturnFromHour = handReturnFromMinute = .5f;
          idleSoundAt = now + 350;
          drawMovie(c, idle, idleOffset, 0, 0);
        }
        break;
      case TOUCH:
        drawTouch(c, touchBack, touchHead, touchFront, elapsed, false);
        break;
      case RELEASE:
        int frame = touchEnd.frameAt(elapsed);
        if (frame < TOUCH_BOUNCE_FRAMES)
          drawTouch(c, touchEndBack, touchEndHead, touchEndFront, elapsed, true);
        else drawMovie(c, touchEnd, elapsed, 0, 0);
        if (elapsed >= touchEnd.duration()) {
          mode = Mode.IDLE;
          stateStarted = now;
          idleOffset = 0;
          pullX = pullY = releasePullX = releasePullY = 0;
          handReturn = true;
          handReturnStarted = now;
          handReturnFromHour = handReturnFromMinute = .5f;
          idleSoundAt = now + 350;
          drawMovie(c, idle, 0, 0, 0);
        }
        break;
      case PANIC:
        long panicElapsed = Math.max(0, elapsed - ALARM_HAND_GATHER_MS);
        drawMovie(c, panic, panicElapsed, 76, -64);
        if (elapsed < ALARM_HAND_GATHER_MS) drawHands(c, now, elapsed);
        else if (alarmView && !alarmAlertPlayed && effects != null) {
          alarmAlertPlayed = true;
          effects.play("alert");
        }
        break;
    }
  }

  private long reactionStartAngry() {
    return smash.duration() - smash.frameStart(SMASH_START_FRAME);
  }

  private long reactionStartAlt() {
    return reactionStartAngry() + angry.duration() * 2L + angry.frameStart(ANGRY_CUT_FRAME);
  }

  private long reactionStartTransition() {
    return reactionStartAlt() + adjustedAltDuration();
  }

  private void drawMovie(
      Canvas c, AnimationAssets.Clip clip, long time, float offsetX, float offsetY) {
    clip.movie.setTime((int) Math.floorMod(time, (long) clip.duration()));
    float centerX = clip == idle ? idleCenterX(time) : idle.centers[0][0];
    float centerY = clip == idle ? idleCenterY(time) : idle.centers[0][1];
    float x = 240 - centerX * CHARACTER_SCALE + offsetX * CHARACTER_SCALE;
    float y = 240 - centerY * CHARACTER_SCALE + offsetY * CHARACTER_SCALE;
    c.save();
    c.translate(x, y);
    c.scale(CHARACTER_SCALE, CHARACTER_SCALE);
    clip.movie.draw(c, 0, 0, paint);
    c.restore();
  }

  private float idleCenterX(long t) {
    return centerAxis(t, 0);
  }

  private float idleCenterY(long t) {
    return centerAxis(t, 1);
  }

  private float centerAxis(long t, int axis) {
    int frame = idle.frameAt(t), next = (frame + 1) % idle.centers.length;
    float within =
        (float) (Math.floorMod(t, (long) idle.duration()) - idle.frameStart(frame))
            / (idle.frameEnd(frame) - idle.frameStart(frame));
    return idle.centers[frame][axis]
        + (idle.centers[next][axis] - idle.centers[frame][axis]) * within;
  }

  private void drawTouch(
      Canvas c,
      AnimationAssets.Clip back,
      AnimationAssets.Clip head,
      AnimationAssets.Clip front,
      long elapsed,
      boolean releasing) {
    int frame = back.frameAt(elapsed);
    back.movie.setTime((int) Math.floorMod(elapsed, (long) back.duration()));
    head.movie.setTime((int) Math.floorMod(elapsed, (long) head.duration()));
    front.movie.setTime((int) Math.floorMod(elapsed, (long) front.duration()));
    float left = 240 - idle.centers[0][0] * CHARACTER_SCALE,
        top = 240 - idle.centers[0][1] * CHARACTER_SCALE;
    Bitmap composite = releasing ? releaseComposite : touchComposite;
    Canvas buffer = releasing ? releaseCompositeCanvas : touchCompositeCanvas;
    buffer.drawColor(Color.TRANSPARENT, PorterDuff.Mode.CLEAR);
    back.movie.draw(buffer, 0, 0, paint);
    float hx = releasing ? touchEndHead.offsetX : touchHead.offsetX,
        hy = releasing ? touchEndHead.offsetY : touchHead.offsetY;
    Bitmap headBitmap = releasing ? releaseHeadBitmap : touchHeadBitmap;
    Canvas headBuffer = releasing ? releaseHeadCanvas : touchHeadCanvas;
    headBuffer.drawColor(Color.TRANSPARENT, PorterDuff.Mode.CLEAR);
    head.movie.draw(headBuffer, HEAD_PADDING, HEAD_PADDING, paint);
    c.save();
    c.translate(left, top);
    c.scale(CHARACTER_SCALE, CHARACTER_SCALE);
    c.drawBitmap(composite, 0, 0, paint);
    c.restore();
    if (mode == Mode.TOUCH || elapsed < touchEnd.frameEnd(TOUCH_BOUNCE_FRAMES - 1))
      drawWarp(c, headBitmap, frame, left, top, hx, hy, releasing);
    c.save();
    c.translate(
        left + (releasing ? touchEndFront.offsetX : touchFront.offsetX) * CHARACTER_SCALE,
        top + (releasing ? touchEndFront.offsetY : touchFront.offsetY) * CHARACTER_SCALE);
    c.scale(CHARACTER_SCALE, CHARACTER_SCALE);
    front.movie.draw(c, 0, 0, paint);
    c.restore();
  }

  private void drawWarp(
      Canvas c,
      Bitmap source,
      int frame,
      float left,
      float top,
      float offsetX,
      float offsetY,
      boolean releasing) {
    float[][] landmarks = touchHead.landmarks;
    int landmarkFrame = releasing ? releaseLandmarkFrame : frame;
    if (landmarkFrame >= landmarks.length) landmarkFrame = landmarks.length - 1;
    if (landmarkFrame < 0) return;
    float cx = HEAD_PADDING + landmarks[landmarkFrame][0],
        cy = HEAD_PADDING + landmarks[landmarkFrame][1],
        radius = touchHead.radius;
    float dx = releasing ? releasePullX : pullX, dy = releasing ? releasePullY : pullY;
    if (Math.abs(dx) + Math.abs(dy) < .1f) {
      c.save();
      c.translate(
          left + (offsetX - HEAD_PADDING) * CHARACTER_SCALE,
          top + (offsetY - HEAD_PADDING) * CHARACTER_SCALE);
      c.scale(CHARACTER_SCALE, CHARACTER_SCALE);
      c.drawBitmap(source, 0, 0, paint);
      c.restore();
      return;
    }
    float displacement = (float) Math.hypot(dx, dy), maxDisplacement = radius * 1.55f;
    if (displacement > maxDisplacement) {
      dx *= maxDisplacement / displacement;
      dy *= maxDisplacement / displacement;
    }
    float influenceRadius = radius * 3.4f;
    for (int row = 0; row <= ROWS; row++) {
      for (int col = 0; col <= COLS; col++) {
        float sx = source.getWidth() * col / COLS, sy = source.getHeight() * row / ROWS;
        float distance = (float) Math.hypot(sx - cx, sy - cy);
        float t = Math.max(0, Math.min(1, 1 - distance / influenceRadius));
        float weight = t * t * (3 - 2 * t);
        int at = (row * (COLS + 1) + col) * 2;
        mesh[at] = sx + (dx * weight);
        mesh[at + 1] = sy + dy * weight;
      }
    }
    c.save();
    c.translate(
        left + (offsetX - HEAD_PADDING) * CHARACTER_SCALE,
        top + (offsetY - HEAD_PADDING) * CHARACTER_SCALE);
    c.scale(CHARACTER_SCALE, CHARACTER_SCALE);
    c.drawBitmapMesh(source, COLS, ROWS, mesh, 0, null, 0, paint);
    c.restore();
  }

  private void drawHands(Canvas c, long now, long elapsed) {
    ZonedDateTime time = ZonedDateTime.now();
    float hour = ((time.getHour() % 12) + time.getMinute() / 60f) * 30;
    float minute = (time.getMinute() + time.getSecond() / 60f) * 6;
    if (mode == Mode.PANIC && elapsed < ALARM_HAND_GATHER_MS) {
      float p = ease(elapsed / (float) ALARM_HAND_GATHER_MS);
      hour = shortTurn(hour, 30, p);
      minute = shortTurn(minute, 30, p);
    } else if (handReturn && now - handReturnStarted < 750) {
      float p = ease((now - handReturnStarted) / 750f);
      hour = shortTurn(handReturnFromHour * 360, hour, p);
      minute = shortTurn(handReturnFromMinute * 360, minute, p);
    } else if (handReturn) handReturn = false;
    // drawMovie normalizes each idle frame's animated center onto (240, 240).
    // Keep both hands on that same registered pivot instead of applying the
    // source-frame center delta a second time.
    drawHand(c, hourHand, 240, 240, hour, 145 * .9f * lengthFactor(0));
    drawHand(c, minuteHand, 240, 240, minute, 169 * .9f * lengthFactor(1));
  }

  private float lengthFactor(int i) {
    return i == 0 ? .95f : 1.05f;
  }

  private void drawHand(Canvas c, Bitmap image, float cx, float cy, float angle, float length) {
    double a = Math.toRadians(angle);
    float tipX = 240 + (float) Math.sin(a) * length, tipY = 240 - (float) Math.cos(a) * length;
    float ux = (tipX - cx) / length, uy = (tipY - cy) / length, thickness = 4.5f;
    float[] src = {240, 240, 240, 240 - (image == hourHand ? 145 : 169), 245, 240};
    float[] dst = {cx, cy, tipX, tipY, cx - uy * thickness, cy + ux * thickness};
    Matrix matrix = new Matrix();
    matrix.setPolyToPoly(src, 0, dst, 0, 3);
    c.drawBitmap(image, matrix, paint);
  }

  private void drawMarkers(Canvas c) {
    float cx = 240, cy = 240;
    if (showTicks) {
      line.setStrokeCap(Paint.Cap.ROUND);
      for (int i = 0; i < 60; i++) {
        if (showNumbers && i % 5 == 0) continue;
        double a = i * Math.PI / 30;
        boolean major = i % 5 == 0;
        line.setStrokeWidth(major ? 6 : 3);
        line.setColor(major ? 0xff435058 : 0xdc54636d);
        float r = DIAL_RADIUS, inner = r - (major ? 28 : 14);
        c.drawLine(
            cx + (float) Math.sin(a) * inner,
            cy - (float) Math.cos(a) * inner,
            cx + (float) Math.sin(a) * r,
            cy - (float) Math.cos(a) * r,
            line);
      }
    }
    if (showNumbers) {
      line.setTypeface(Typeface.create("sans-serif", Typeface.NORMAL));
      line.setTextSize(32);
      line.setTextAlign(Paint.Align.CENTER);
      line.setStrokeWidth(5);
      line.setStrokeJoin(Paint.Join.ROUND);
      line.setStyle(Paint.Style.STROKE);
      line.setColor(Color.WHITE);
      for (int i = 1; i <= 12; i++) {
        double a = i * Math.PI / 6;
        float x = cx + (float) Math.sin(a) * (DIAL_RADIUS - 28),
            y =
                cy
                    - (float) Math.cos(a) * (DIAL_RADIUS - 28)
                    - (line.ascent() + line.descent()) / 2;
        c.drawText(Integer.toString(i), x, y, line);
      }
      line.setStyle(Paint.Style.FILL);
      line.setColor(0xff323f48);
      for (int i = 1; i <= 12; i++) {
        double a = i * Math.PI / 6;
        float x = cx + (float) Math.sin(a) * (DIAL_RADIUS - 28),
            y =
                cy
                    - (float) Math.cos(a) * (DIAL_RADIUS - 28)
                    - (line.ascent() + line.descent()) / 2;
        c.drawText(Integer.toString(i), x, y, line);
      }
    }
  }

  private void drawStars(Canvas c, long now) {
    if (smashAt == 0) return;
    long elapsed = now - smashAt;
    if (elapsed >= 650) {
      smashAt = 0;
      return;
    }
    float progress = elapsed / 650f;
    for (int i = 0; i < STAR_COUNT; i++) {
      float local = Math.max(0, Math.min(1, (progress - starDelay[i]) / (1 - starDelay[i])));
      if (progress <= starDelay[i]) continue;
      float appear = Math.min(1, local * 7), radius = starSize[i] * appear * (1 - local * .45f);
      float x = lastTouchX + starVx[i] * local,
          y = lastTouchY + starVy[i] * local + starGravity[i] * local * local;
      star.setColor(Color.argb((int) (255 * (1 - local)), 255, 221, 35));
      star.setStyle(Paint.Style.FILL);
      Path path = new Path();
      for (int j = 0; j < 10; j++) {
        double a = starRotation[i] + local * 1.8 - Math.PI / 2 + j * Math.PI / 5;
        float r = radius * (j % 2 == 0 ? 1 : .42f);
        float px = x + (float) Math.cos(a) * r, py = y + (float) Math.sin(a) * r;
        if (j == 0) path.moveTo(px, py);
        else path.lineTo(px, py);
      }
      path.close();
      c.drawPath(path, star);
    }
  }

  private void beginStars(float x, float y) {
    smashAt = SystemClock.uptimeMillis();
    float[] base = {-63, -39, -17, 17, 39, 63};
    for (int i = 0; i < STAR_COUNT; i++) {
      float variation = random.nextFloat() * 2 - 1,
          height = 10 + random.nextFloat() * 30,
          fall = 25 + random.nextFloat() * 25;
      starX[i] = x;
      starY[i] = y;
      starVx[i] = base[i] + variation * (i == 0 || i == 5 ? 5 : 9);
      float upward = 2 * height + 2 * (float) Math.sqrt(height * (height + fall));
      starVy[i] = -upward;
      starGravity[i] = upward + fall;
      starSize[i] = 14.25f + variation * 3;
      starDelay[i] = random.nextFloat() * .03f;
      starRotation[i] = (random.nextFloat() * 2 - 1) * (float) Math.PI;
    }
    lastTouchX = x;
    lastTouchY = y;
  }

  private float ease(float p) {
    p = Math.max(0, Math.min(1, p));
    return p * p * (3 - 2 * p);
  }

  private float shortTurn(float start, float target, float p) {
    float d = ((target - start + 540) % 360) - 180;
    return start + d * ease(p);
  }

  private long nextDelay(long now) {
    long elapsed = now - stateStarted;
    AnimationAssets.Clip current = idle;
    if (mode == Mode.SMASH) {
      int stage = reactionStage(elapsed);
      current =
          stage == 0
              ? smash
              : stage == 1 ? angry : stage == 2 ? angryAlt : stage == 3 ? transition : idle;
    } else if (mode == Mode.TOUCH) current = touchBack;
    else if (mode == Mode.RELEASE) current = touchEnd;
    else if (mode == Mode.PANIC) current = panic;
    int frame =
        current.frameAt(
            mode == Mode.SMASH && reactionStage(elapsed) == 0
                ? elapsed + smash.frameStart(SMASH_START_FRAME)
                : elapsed);
    long remain =
        current.frameEnd(frame)
            - Math.floorMod(
                mode == Mode.IDLE ? idleOffset + elapsed : elapsed, (long) current.duration());
    return Math.max(16, Math.min(50, remain));
  }

  private void maybePlayIdle(long now) {
    if (mode == Mode.IDLE && idleSoundAt != Long.MAX_VALUE && now >= idleSoundAt) {
      idleSoundAt = Long.MAX_VALUE;
      if (effects != null) effects.play(random.nextBoolean() ? "idle1" : "idle2");
    }
  }

  private void returnToIdle(boolean animate) {
    mode = Mode.IDLE;
    stateStarted = SystemClock.uptimeMillis();
    idleOffset = 0;
    pullX = pullY = releasePullX = releasePullY = 0;
    handReturn = animate;
    handReturnStarted = stateStarted;
    handReturnFromHour = handReturnFromMinute = 1f / 12f;
    idleSoundAt = stateStarted + 300;
  }

  @Override
  public boolean performClick() {
    super.performClick();
    triggerSmash();
    return true;
  }

  @Override
  public boolean onTouchEvent(MotionEvent event) {
    float size = Math.min(getWidth(), getHeight());
    if (size <= 0) return false;
    float x = (event.getX() - (getWidth() - size) / 2f) * 480f / size,
        y = (event.getY() - (getHeight() - size) / 2f) * 480f / size;
    if (event.getActionMasked() == MotionEvent.ACTION_DOWN) {
      if (mode == Mode.PANIC) return true;
      if (Math.hypot(x - 240, y - 240) <= 65) {
        performClick();
        return true;
      }
      float centerX = 240 + (transition.pointX - idle.centers[0][0]) * CHARACTER_SCALE;
      float centerY = 240 + (transition.pointY - idle.centers[0][1]) * CHARACTER_SCALE;
      if (Math.hypot(x - centerX, y - centerY) <= 40) {
        startTouch(x, y);
        return true;
      }
      return false;
    }
    if (mode == Mode.TOUCH && event.getActionMasked() == MotionEvent.ACTION_MOVE) {
      pullX = (x - lastTouchX) / CHARACTER_SCALE;
      pullY = (y - lastTouchY) / CHARACTER_SCALE;
      float distance = (float) Math.hypot(pullX, pullY), limit = touchHead.radius * 1.8f;
      if (distance > limit) {
        pullX *= limit / distance;
        pullY *= limit / distance;
      }
      invalidate();
      return true;
    }
    if (mode == Mode.TOUCH
        && (event.getActionMasked() == MotionEvent.ACTION_UP
            || event.getActionMasked() == MotionEvent.ACTION_CANCEL)) {
      finishTouch();
      return true;
    }
    if ((event.getActionMasked() == MotionEvent.ACTION_UP
            || event.getActionMasked() == MotionEvent.ACTION_CANCEL)
        && getParent() != null) getParent().requestDisallowInterceptTouchEvent(false);
    return true;
  }
}
