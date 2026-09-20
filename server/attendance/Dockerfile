FROM python:3.12-slim

WORKDIR /app
COPY requirements.txt .
# pypi.org از ایران معمولاً تایم‌اوت می‌دهد - میرور ایرانی پیش‌فرض است؛
# اگر از کار افتاد: pypi.iran.repo.ir/simple یا mirrors.aliyun.com/pypi/simple
ARG PIP_INDEX_URL=https://mirror-pypi.runflare.com/simple
RUN pip install --no-cache-dir --index-url ${PIP_INDEX_URL} -r requirements.txt

COPY app.py db.py integrations.py jdate.py ./

# دیتابیس در volume می‌رود (SQLITE_PATH) - کاربر non-root
RUN useradd -m server
USER server

EXPOSE 8000
CMD ["uvicorn", "app:app", "--host", "0.0.0.0", "--port", "8000"]
