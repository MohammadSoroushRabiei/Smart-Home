# docker.io از ایران ۴۰۳ می‌دهد - میرور ایرانی پیش‌فرض است؛
# اگر از کار افتاد: registry.docker.ir
ARG DOCKER_MIRROR=docker.arvancloud.ir
FROM ${DOCKER_MIRROR}/library/python:3.12-slim

WORKDIR /app
COPY requirements.txt .
# pypi.org از ایران معمولاً تایم‌اوت می‌دهد - میرور ایرانی پیش‌فرض است؛
# اگر از کار افتاد: pypi.iran.repo.ir/simple یا mirrors.aliyun.com/pypi/simple
ARG PIP_INDEX_URL=https://mirror-pypi.runflare.com/simple
RUN pip install --no-cache-dir --index-url ${PIP_INDEX_URL} -r requirements.txt

COPY bot.py .
ENV PYTHONUNBUFFERED=1
CMD ["python", "bot.py"]
