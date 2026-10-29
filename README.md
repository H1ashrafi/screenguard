# ScreenGuard (C++, Win32)

Locks the physical keyboard and mouse with `Ctrl+Shift+X` while injected input (automation, downloaders) keeps working. Shows a tiny red dot at the top of the screen while locked, keeps the laptop awake, and saves a webcam photo each time it locks and unlocks (kept locally in `AccessLogs/`, which is git-ignored). No network access, no keys or credentials. Persian documentation below.

---

# ScreenGuard

قفل ورودی فیزیکی برای ویندوز. با `Ctrl+Shift+X` هم‌زمان، کیبورد و ماوسِ فیزیکی
کامل مسدود می‌شوند؛ ولی ورودی‌های تزریق‌شده‌ی خودِ لپتاپ (اتوماسیون، دانلودر،
اسکریپت‌هایی که ماوس را حرکت می‌دهند) عبور می‌کنند و کارشان را ادامه می‌دهند.
با همان `Ctrl+Shift+X` به حالت عادی برمی‌گردد. لپتاپ تا وقتی برنامه روشن است
بیدار می‌ماند.

## مکانیزم

- دو هوک سراسری `WH_KEYBOARD_LL` و `WH_MOUSE_LL`.
- هر رویداد فلگ injected دارد. فیزیکی = injected صفر، اتوماسیون = injected یک.
- در حالت قفل: رویداد فیزیکی بلاک، رویداد injected عبور.
- ترکیب قفل فقط با کلید فیزیکی می‌سوزد؛ اتوماسیون هرگز قفل را نمی‌اندازد/برنمی‌دارد.
- هنگام قفل، یک نقطه‌ی قرمزِ کوچک وسطِ بالای صفحه دیده می‌شود. بدون بالن/نوتیفیکیشن.
- بیدارماندن با `SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED)`.
  خوابِ بی‌کاری و خاموشیِ صفحه را می‌گیرد، ولی خاموشیِ حرارتی و باتریِ بحرانی را
  دور نمی‌زند.

## چرا Ctrl+Shift+X و نه Shift+Z+X

کیبوردِ این لپتاپ سه کلیدِ `Shift`+`Z`+`X` را با هم گزارش نمی‌کند (ghosting /
2-key rollover). با لاگ ثابت شد هر جفت کار می‌کند ولی هر سه با هم هرگز نمی‌رسند.
مادیفایرها (Ctrl/Shift) خطِ اختصاصی دارند و این مشکل را ندارند.

## ساخت

```bash
build.bat
```

یا مستقیم:

```bash
g++ -std=c++17 -O2 -DUNICODE -D_UNICODE -Wall -Wextra main.cpp -o ScreenGuard.exe -mwindows -static -static-libgcc -static-libstdc++ -lgdi32 -luser32 -lshell32
```

## اجرا

`ScreenGuard.exe` را باز کن. یک آیکون در system tray می‌آید (سبز = باز، قرمز = قفل).
راست‌کلیک روی آیکون: قفل/بازکردن دستی و خروج.

- قفل کردن: `Ctrl+Shift+X` هم‌زمان. یک نقطه‌ی قرمز وسطِ بالای صفحه ظاهر می‌شود.
- بازکردن: دوباره `Ctrl+Shift+X` هم‌زمان.

نیازی به ادمین نیست. تنها استثنا: اگر یک پنجره‌ی بالاآمده‌ی ادمین (elevated) روی
فوکوس باشد، نسخه‌ی غیرادمین آن یک پنجره را نمی‌بیند؛ در آن صورت با
«Run as administrator» اجرا کن.

## آرگومان‌های خط فرمان

- `/autolock` — ۲ ثانیه بعد از باز شدن، خودش قفل می‌کند (برای اجرای خودکار در بوت).
- `/debug` — رویدادها را در `%TEMP%\screenguard_debug_log.txt` می‌نویسد و بلاک را
  خاموش می‌کند (فقط برای عیب‌یابی؛ در حالت عادی از این استفاده نکن).

## راه فرار همیشگی

`Ctrl+Alt+Del` را هیچ برنامه‌ی کاربری در ویندوز نمی‌تواند بگیرد. از آن‌جا Task
Manager را باز کن و ScreenGuard را ببند؛ با بسته‌شدن پروسه، هوک‌ها خودکار برداشته
و ورودی آزاد می‌شود.

## محدودیت

اتوماسیونی که ورودی را در سطحِ درایورِ سخت‌افزاری تولید می‌کند (نه از راه
`SendInput`/`mouse_event`) فلگ injected ندارد و در حالت قفل بلاک می‌شود.
ابزارهای معمولِ حرکت‌دهنده‌ی ماوس و اتوکلیکرها از `SendInput` استفاده می‌کنند و
عبور می‌کنند.

## License

Free for personal, educational and research use. **Commercial use (selling, publishing the app in a store, or using it in a paid product) is not allowed without written permission.** See `LICENSE` (PolyForm Noncommercial 1.0.0).
