# The CalcGPT cloud server. Built by cloud/deploy.sh from a small folder that holds only cloud/ and calcgpt/mac/
# (it also builds from the project folder:  docker build -f cloud/Dockerfile .). Never copies .env or devices.json.
FROM python:3.13-slim

# time zones, so the system prompt can tell the model the local date and time (TZ is set in fly.toml)
RUN apt-get update && apt-get install -y --no-install-recommends tzdata && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY calcgpt/mac/calcgptd.py calcgpt/mac/models.json calcgpt/mac/
# cloud/models.json is optional: the [n] makes the pattern match nothing when it is missing
COPY cloud/server.py cloud/add_device.py cloud/models.jso[n] cloud/

# devices.json and usage.json live on a mounted volume, so they survive deploys and restarts
ENV CALCGPT_DEVICES=/data/devices.json \
    PYTHONUNBUFFERED=1 \
    PYTHONDONTWRITEBYTECODE=1
EXPOSE 7979
CMD ["python3", "cloud/server.py", "--port", "7979"]
