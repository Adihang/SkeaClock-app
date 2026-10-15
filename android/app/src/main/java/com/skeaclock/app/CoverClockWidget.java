package com.skeaclock.app;

/** Samsung Flex Window version of the clock widget. */
public final class CoverClockWidget extends ClockWidget {
  private static final int PENDING_INTENT_REQUEST_CODE_OFFSET = 100;

  @Override
  protected int pendingIntentRequestCode(int requestCode) {
    return requestCode + PENDING_INTENT_REQUEST_CODE_OFFSET;
  }
}
