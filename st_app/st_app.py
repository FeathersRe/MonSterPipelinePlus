import os
import tempfile
import requests
import streamlit as st
from PIL import Image
from io import BytesIO
from storage import upload_to_storage

MONSTER_URL = "http://monster:8003/detect"

def st_app():
    st.set_page_config(page_title="MonSter Estimation pipeline")
    st.header("🖼️MonSter Estimation pipeline")
    
    run = st.button("Generate")

    with st.sidebar:
        uploads = st.file_uploader(
            accept_multiple_files=False,
            label="Upload image here:",
            type=["jpg", "jpeg", "png"]   
        )

        if uploads:
            with st.expander("View image"):
                st.image(uploads)

    if uploads and run:
        """ 
            temp_file = tempfile.NamedTemporaryFile(
            "wb", suffix=f".{uploads.type.split('/')[1]}", delete=False
        )
        temp_file.write(uploads.getbuffer())
        image_path = temp_file.name 
        """
        filename = uploads.name
        image_url = upload_to_storage(uploads, filename)


        with st.spinner("Running..."):
            """             
            with open(image_path, "rb") as f:
                files = {"image": (image_path, f, uploads.type)}
                data = {"prompt": user_req}
                response = requests.post(MODEL_LINKS[option], files=files, data=data) 
            """
            data = {
                "image_url" : image_url
            }
            response = requests.post(MONSTER_URL, data=data)

            if response.status_code == 200:
                plotted_img = Image.open(BytesIO(response.content))
                st.image(plotted_img)
            else:
                st.error("Failed to get image from backend.")
                
if __name__ == "__main__":
    st_app()