# Base image with Python 3.8
FROM python:3.8

# Prevents prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install system-level dependencies
RUN apt-get update && apt-get install -y \
    git \
    ffmpeg \
    libsm6 \
    libxext6 \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /app

# Copy depedency list
COPY requirements.txt .

#Install Python depedencies from requirements.txt
RUN pip install --upgrade pip && \
    pip install -r requirements.txt

RUN pip install mmcv==2.1.0 -f https://download.openmmlab.com/mmcv/dist/cu118/torch2.1/index.html

# Copy project files
COPY . .

# Default command
CMD [ "python3" ]

