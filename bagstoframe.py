import os
import sys

import capnp
import cv2
import numpy as np
import argparse

from mcap.reader import make_reader

sys.path.append(os.path.join(os.path.dirname(__file__), "./vk_sdk/capnp"))
sys.path.append('/opt/vilota/messages')

import image_capnp as eCALImage

def inspect_mcap(args):
    """
    Helper function querying details of mcap file
    
    :param args.mcap_path: path of the mcap file
    """
    mcap_path = args.mcap_path
    
    with open(mcap_path, "rb") as file:
        reader = make_reader(file)
        summary = reader.get_summary()
        
        print("Topics in MCAP:")
        for channel in summary.channels.values():
            print(f"Topics: {channel.topic}, Message Encoding: {channel.message_encoding}, Schema ID: {channel.schema_id}")

def decode_image_msg(msg):
    """
    Decodes mcap message packets to image data.
    Uses image_capnp as image decoding schema

    :param msg: message packets extracted from mcap
    """
    with eCALImage.Image.from_bytes(msg) as img:
        encoding = img.encoding
        width = img.width
        height = img.height
        data = img.data

        if encoding == "mono8":
            img_np = np.frombuffer(data, dtype=np.uint8).reshape((height, width))
        elif encoding == "mono16":
            img_np = np.frombuffer(data, dtype=np.uint16).reshape((height, width)) 
        elif encoding == "bgr8":
            img_np = np.frombuffer(data, dtype=np.uint8).reshape((height, width, 3))
        elif encoding == "jpeg" or "png":
            img_np = cv2.imdecode(np.frombuffer(data, dtype=np.uint8), cv2.IMREAD_UNCHANGED)
        else:
            raise ValueError(f"Unsupported image encoding: {encoding}")

        return img_np

def save_camera_frames(args):
    """
    Saves the frames of a particular bag file as png images

    :param args.mcap_path: path of the mcap file
    :param args.img_outdir: output path of the frame images
    :param 
    """

    mcap_path = args.mcap_path
    img_out = args.img_outdir
    start_offset_sec = float(args.time_offset)
    time_step_sec = float(args.time_step)
    frame_count = int(args.frame_count)

    left_img_path = os.path.join(img_out, "image_2")
    right_img_path = os.path.join(img_out, "image_3")

    if not os.path.exists(left_img_path):
        os.mkdir(left_img_path)
    
    elif not os.path.exists(right_img_path):
        os.mkdir(right_img_path)

    left_img_topic = "S1/stereo1_l"
    right_img_topic = "S1/stereo2_r"

    with open(mcap_path, "rb") as file:
        reader = make_reader(file)

        left_img_iter = reader.iter_messages(topics=["S1/stereo1_l"])
        right_img_iter = reader.iter_messages(topics=["S1/stereo2_r"])

        count = 0
        last_captured_time = 0
        next_left_msg = next(left_img_iter, None)
        next_right_msg = next(right_img_iter, None)

        cutoff_time_l_ns=cutoff_time_r_ns=None

        #Infinite loop: Time precision incorrect
        while next_left_msg and next_right_msg and count < frame_count:
            _, _, msgl = next_left_msg
            _, _, msgr = next_right_msg
            
            if cutoff_time_l_ns == None or cutoff_time_r_ns == None:
                cutoff_time_l_ns = msgl.log_time + int(start_offset_sec * 1e9)
                cutoff_time_r_ns = msgr.log_time + int(start_offset_sec * 1e9)
                time_step_ns = int(time_step_sec * 1e9)

            if msgl.log_time < cutoff_time_l_ns or msgr.log_time < cutoff_time_r_ns:
                if msgl.log_time < cutoff_time_l_ns:
                    next_left_msg = next(left_img_iter, None)

                if msgr.log_time < cutoff_time_r_ns:
                    next_right_msg = next(right_img_iter, None)
                
                continue

            if msgl.log_time > last_captured_time + time_step_ns:
                    imgl = decode_image_msg(msgl.data)
                    imgr = decode_image_msg(msgr.data)

                    cv2.imwrite(os.path.join(left_img_path, f"left_{count:04d}.png"), imgl)
                    cv2.imwrite(os.path.join(right_img_path, f"right_{count:04d}.png"), imgr)
                    
                    last_captured_time = msgl.log_time
                    count += 1

            next_left_msg = next(left_img_iter, None)
            next_right_msg = next(right_img_iter, None)     

        print(f"Extracted {count} image pairs to {img_out}")

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--mcap_path", help="path of mcap file", default="./input_bags/")
    parser.add_argument("--img_outdir", help="output directory of bag image", default="./input_imgs/")
    parser.add_argument("--time_offset", help="duration offset (in secs) from start to capture frames from", default=0)
    parser.add_argument("--time_step", help="time step (in s) between the bag frames captured", default=1)
    parser.add_argument("--frame_count", help="number of frames of bag to capture", default=5)

    args = parser.parse_args()

    inspect_mcap(args)
    save_camera_frames(args)

if __name__ == "__main__":
    main()