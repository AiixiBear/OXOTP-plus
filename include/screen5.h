// OTP list screen: all stored codes at a glance.
//
// Each row shows one stored OTP: its label on the first line, the current
// code on the second. The selected row is drawn inverted and also shows
// its position ("n/m") and the seconds left in the current 30s window.
//
// BtnA short press moves the selection down (wraps to the first entry),
// BtnA long press types the selected code over BLE, BtnB moves to the
// next screen (handled by switchscreen, as on the other screens).

#define OTP_LIST_ROW_H 28

void OTP_list_screen() {
  int ids[maxOTPs];
  int how_many = 0;
  for (int i = 1; i <= maxOTPs; i++) {
    if (NVS.getInt("B" + String(i)) == 1) {
      ids[how_many++] = i;
    }
  }

  // Rows that fit below the toolbar: 4 on the Plus2 (118px), 2 on the
  // M5StickC (63px). The window scrolls so the selection stays visible.
  int visibleRows = (screen_y - toolbar_height) / OTP_LIST_ROW_H;
  int sel = 0;                 // index into ids[]
  int winStart = 0;            // first visible row

  // Codes are cached per 30s step: recomputing every HMAC on every loop
  // iteration is wasted work, and the codes only change when the step
  // rolls over. The cache lives only for one visit to this screen, so a
  // code can never survive a WebUI edit.
  long cachedStep = -1;
  String cachedCodes[maxOTPs];

  bool needRepaint = true;
  int lastSec = -1;

  while (switchscreen() == false) {

    if (how_many == 0) {
      if (needRepaint) {
        needRepaint = false;
        M5.Lcd.fillRect(0, toolbar_height, screen_x, screen_y - toolbar_height, bg_color);
        M5.Lcd.setFont(&beta5pt7b);
        M5.Lcd.setTextColor(txt_color, bg_color);
        M5.Lcd.setCursor(10, 32);
        M5.Lcd.print("NO OTPs");
        M5.Lcd.setCursor(10, 43);
        M5.Lcd.print("GO TO WIFI SETUP");
        M5.Lcd.setFont(&beta8pt7b);
        M5.Lcd.setCursor(10, 60);
        M5.Lcd.print("OXOTP+");
      }
      continue;
    }

    long currentStep = now() / OTP_TIME_STEP;
    if (currentStep != cachedStep) {
      for (int i = 0; i < how_many; i++) {
        size_t hmac_length = NVS.getBlobSize(String(ids[i]));
        uint8_t hmacKey[hmac_length] = {};
        NVS.getBlob(String(ids[i]), hmacKey, hmac_length);
        cachedCodes[i] = generateTOTP(hmacKey, hmac_length, currentStep * OTP_TIME_STEP, getOTPDigits(ids[i]));
      }
      cachedStep = currentStep;
      needRepaint = true;
    }

    // Short press moves the selection down, wrapping to the first entry.
    if (M5.BtnA.wasClicked()) {
      previousMillis = millis();
      sel = (sel + 1) % how_many;
      if (sel < winStart) {
        winStart = sel;
      }
      if (sel >= winStart + visibleRows) {
        winStart = sel - visibleRows + 1;
      }
      needRepaint = true;
    }

    // Long press types the selected code; typeCodeOverBLE reports on
    // screen, so repaint the list once it returns.
    if (M5.BtnA.wasHold()) {
      previousMillis = millis();
      typeCodeOverBLE(cachedCodes[sel]);
      needRepaint = true;
    }

    if (needRepaint) {
      needRepaint = false;
      lastSec = second();
      M5.Lcd.fillRect(0, toolbar_height, screen_x, screen_y - toolbar_height, bg_color);

      for (int r = 0; r < visibleRows; r++) {
        int idx = winStart + r;
        if (idx >= how_many) {
          break;
        }
        int rowTop = toolbar_height + r * OTP_LIST_ROW_H;
        bool selected = (idx == sel);
        if (selected) {
          M5.Lcd.fillRect(0, rowTop, screen_x, OTP_LIST_ROW_H, txt_color);
        }

        // Label line. The selected row also shows its position, e.g. "2/7".
        M5.Lcd.setTextColor(selected ? bg_color : txt_color, selected ? txt_color : bg_color);
        M5.Lcd.setFont(&beta8pt7b);
        M5.Lcd.setCursor(5, rowTop + 2);
        String label = NVS.getString("L" + String(ids[idx]));
        int maxLabelChars = (current_screen == STICKC) ? 14 : 24;
        M5.Lcd.print(label.substring(0, maxLabelChars));
        if (selected) {
          String pos = String(idx + 1) + "/" + String(how_many);
          M5.Lcd.setCursor(screen_x - 5 - M5.Lcd.textWidth(pos), rowTop + 2);
          M5.Lcd.print(pos);
        }

        // Code line. The selected row also shows the seconds left in the
        // current window, e.g. "12s".
        if (current_screen == STICKC) {
          M5.Lcd.setFont(&beta8pt7b);
        } else {
          M5.Lcd.setFont(&beta10pt7b);
        }
        M5.Lcd.setCursor(5, rowTop + 15);
        M5.Lcd.print(cachedCodes[idx]);
        if (selected) {
          String rem = String(OTP_TIME_STEP - (second() % OTP_TIME_STEP)) + "s";
          M5.Lcd.setFont(&beta8pt7b);
          M5.Lcd.setCursor(screen_x - 5 - M5.Lcd.textWidth(rem), rowTop + 15);
          M5.Lcd.print(rem);
        }
      }
    }

    // Refresh the countdown once a second by redrawing only the seconds
    // value on the selected row. The erase covers the widest possible
    // value ("30s") so a shrinking value leaves no remnants behind.
    if (second() != lastSec) {
      lastSec = second();
      int r = sel - winStart;
      if (r >= 0 && r < visibleRows) {
        int rowTop = toolbar_height + r * OTP_LIST_ROW_H;
        M5.Lcd.setFont(&beta8pt7b);
        int maxRemW = M5.Lcd.textWidth("30s");
        M5.Lcd.fillRect(screen_x - 5 - maxRemW, rowTop + 15, maxRemW, 12, txt_color);
        String rem = String(OTP_TIME_STEP - (second() % OTP_TIME_STEP)) + "s";
        M5.Lcd.setTextColor(bg_color, txt_color);
        M5.Lcd.setCursor(screen_x - 5 - M5.Lcd.textWidth(rem), rowTop + 15);
        M5.Lcd.print(rem);
        M5.Lcd.setTextColor(txt_color, bg_color);
      }
    }
  }
}
