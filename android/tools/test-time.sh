#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT
"${JAVA_HOME:+$JAVA_HOME/bin/}javac" -d "$out" app/src/main/java/com/skeaclock/app/AlarmTime.java tests/AlarmTimeTest.java
"${JAVA_HOME:+$JAVA_HOME/bin/}java" -cp "$out" com.skeaclock.app.AlarmTimeTest
