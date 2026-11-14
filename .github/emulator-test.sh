#!/bin/bash
# Installs the test APK in the running emulator, starts the game and records what happens.
mkdir -p /tmp/logs
PKG=org.openttdfork.game

adb install -r apks/openttd-fork-test.apk 2>&1 | tee /tmp/logs/emu-install.log
adb logcat -c
adb shell monkey -p $PKG -c android.intent.category.LAUNCHER 1 2>&1 | tee /tmp/logs/emu-start.log

alive=0
for t in 10 20 30 45 60 75 90; do
  sleep 15
  if adb shell pidof $PKG > /dev/null 2>&1 && [ -n "$(adb shell pidof $PKG | tr -d '\r')" ]; then alive=1; else alive=0; fi
  echo "after ~${t}s alive=$alive" | tee -a /tmp/logs/emu-alive.log
done

adb logcat -d -t 3000 > /tmp/logs/emu-logcat-full.txt
grep -iE "openttd|SDL|libmain|AndroidRuntime|FATAL|DEBUG|crash|signal|UnsatisfiedLink|Exception" /tmp/logs/emu-logcat-full.txt | tail -n 300 > /tmp/logs/emu-logcat.txt
adb exec-out screencap -p > /tmp/logs/emu-screen.png
adb shell run-as $PKG ls -la files files/data/openttd files/config/openttd > /tmp/logs/emu-files.txt 2>&1
adb shell run-as $PKG cat files/crash.txt > /tmp/logs/emu-crash.txt 2>&1
adb shell run-as $PKG tail -n 80 files/openttd-stderr.txt > /tmp/logs/emu-stderr.txt 2>&1
rm /tmp/logs/emu-logcat-full.txt

# The game must still be running at the end.
[ "$alive" = "1" ]
