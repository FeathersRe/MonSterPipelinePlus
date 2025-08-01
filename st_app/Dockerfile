FROM python:3.10.12

ENV DEBIAN_FRONTEND=noninteractive

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

# Copy project files
COPY . .

EXPOSE 8501

# Default command
CMD ["streamlit", "run", "labellingpipeline_st.py", "--server.port=8501", "--server.address=0.0.0.0"]