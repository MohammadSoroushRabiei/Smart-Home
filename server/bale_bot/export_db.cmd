@echo off
rem خروجی گرفتن از دیتابیس ربات بله برای مشاهده در VS Code
rem دیتابیس داخل volume داکری است (bale_data) - این اسکریپت یک کپی سالم
rem از آن را با sqlite3 .backup می‌سازد و به data\bale_bot_live.db می‌آورد.
rem هر بار بعد از اجرای این فایل، در VS Code فایل live را دوباره باز/رفرش کن.

docker exec bale-bot python -c "import sqlite3; s=sqlite3.connect('/data/bale_bot.db'); d=sqlite3.connect('/data/export.db'); s.backup(d); d.close()"
if errorlevel 1 (
  echo ERROR: could not read the database - is the bale-bot container running?
  exit /b 1
)
docker cp bale-bot:/data/export.db "%~dp0data\bale_bot_live.db"
docker exec bale-bot python -c "import os; os.remove('/data/export.db')"
echo OK - open server\bale_bot\data\bale_bot_live.db in VS Code
