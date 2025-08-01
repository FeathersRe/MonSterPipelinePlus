import os
import boto3
from botocore.client import Config
from dotenv import load_dotenv

load_dotenv()
BUCKET_NAME = os.getenv("BUCKET_NAME")
ACCESS_ID = os.getenv("ACCESS_ID")
ACCESS_KEY = os.getenv("ACCESS_KEY")
ENDPOINT_URL = os.getenv("ENDPOINT_URL")

s3 = boto3.client('s3',
                  endpoint_url=ENDPOINT_URL,
                  aws_access_key_id=ACCESS_ID,
                  aws_secret_access_key=ACCESS_KEY,
                  config=Config(signature_version='s3v4'),
                  region_name='any')

s3.create_bucket(Bucket=BUCKET_NAME)

def upload_to_storage(image_file, filename):
    s3.upload_fileobj(image_file, BUCKET_NAME, filename, ExtraArgs={'ContentType': image_file.type})
    return f"{ENDPOINT_URL}/{BUCKET_NAME}/{filename}"