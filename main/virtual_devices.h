#pragma once

#include <stdbool.h>

/**
 * @brief راه‌اندازی تسک دستگاه‌های مجازی: سنسور حضور و سنسور نور محیط.
 *        هر دو «فرضی» هستند (بدون سخت‌افزار) و با الگوی زمانی واقع‌گرایانه
 *        شبیه‌سازی می‌شوند تا بتوان مدل ML را بدون سنسور فیزیکی نمایش داد.
 *        مقادیر به‌صورت دوره‌ای روی MQTT هم منتشر می‌شوند.
 */
void virtual_devices_start(void);

/**
 * @brief هر باز شدن موفق قفل (چهره/رمز) یعنی کاربر واقعاً رسیده:
 *        حضور برای ~۴۵ دقیقه به‌صورت قطعی «خانه» می‌شود و بازه‌ی غیبت
 *        برنامه‌ای فعلی مختل می‌شود.
 */
void virtual_devices_notify_user_seen(void);

/**
 * @brief snapshot مقادیر فعلی (thread-safe).
 * @return false اگر هنوز ساعت تنظیم نشده و مقادیر اولیه‌اند
 */
bool virtual_devices_get_env(bool *presence, float *lux);
