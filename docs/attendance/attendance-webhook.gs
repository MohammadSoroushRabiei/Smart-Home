/**
 * حضور و غیاب — Google Apps Script webhook
 *
 * سرور لوکال (داکر) رویدادها را به این اسکریپت می‌فرستد و این اسکریپت
 * ردیف‌ها را در همین شیت اضافه می‌کند.
 *
 * راه‌اندازی:
 *   1. در شیت: Extensions → Apps Script → محتوای این فایل را پیست کن
 *   2. مقدار SECRET را عوض کن و همان را در .env سرور (GOOGLE_SCRIPT_SECRET)
 *      بگذار
 *   3. Deploy → New deployment → Web app → Execute as: Me → Access: Anyone
 *   4. آدرس /exec تولیدشده را در .env سرور (GOOGLE_SCRIPT_URL) بگذار
 *
 * ستون‌ها (خودکار ساخته می‌شوند):
 *   تاریخ | ساعت | نام | نوع
 */

var SECRET = 'secret_';

var TYPE_LABELS = {
  attendance_in: 'ورود',
  attendance_out: 'خروج',
  door_face: 'باز شدن درب (چهره)',
  door_code: 'باز شدن درب (رمز)'
};

function doPost(e) {
  try {
    if (!e || !e.postData || !e.postData.contents) {
      return respond({ ok: false, error: 'no body' });
    }
    var data = JSON.parse(e.postData.contents);

    if (data.secret !== SECRET) {
      return respond({ ok: false, error: 'bad secret' });
    }

    // پیام تست اتصال از داشبورد برد — ردیف نمی‌نویسد
    if (data.event === 'test') {
      return respond({ ok: true, message: 'test ok' });
    }

    var sheet = SpreadsheetApp.getActiveSpreadsheet().getActiveSheet();

    // هدرها فقط بار اول نوشته می‌شوند
    if (sheet.getLastRow() === 0) {
      sheet.appendRow(['تاریخ', 'ساعت', 'نام', 'نوع']);
      sheet.getRange(1, 1, 1, 4).setFontWeight('bold');
    }

    var typeLabel = TYPE_LABELS[data.event] || data.event;
    sheet.appendRow([
      data.date || '',
      data.time || '',
      data.name || '',
      typeLabel
    ]);

    // قالب سلول‌های تاریخ/ساعت صریحاً متن می‌شود تا قالب‌بندی خودکار شیت
    // (مثلاً ستونی که قبلاً در آن تاریخ/عدد بوده) مقادیر را خراب نکند
    var row = sheet.getLastRow();
    sheet.getRange(row, 1).setNumberFormat('@');   // تاریخ - متن
    sheet.getRange(row, 2).setNumberFormat('@');   // ساعت - متن

    return respond({ ok: true });
  } catch (err) {
    return respond({ ok: false, error: String(err) });
  }
}

function respond(obj) {
  return ContentService
    .createTextOutput(JSON.stringify(obj))
    .setMimeType(ContentService.MimeType.JSON);
}
