import argparse
import os
import capnp
import cv2
import numpy as np
from mcap.reader import make_reader

image_capnp = capnp.load("./encoder/image.capnp")

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
    img = image_capnp.Image.from_bytes_packed(msg)
    encoding = img.encoding
    width = img.width
    height = img.height
    data = img.data

    if encoding == image_capnp.Image.Encoding.mono8:
        img_np = np.frombuffer(data, dtype=np.uint8).reshape((height, width))
    elif encoding == image_capnp.Image.Encoding.mono16:
        img_np = np.frombuffer(data, dtype=np.uint16).reshape((height, width)) 
    elif encoding == image_capnp.Image.Encoding.bgr8:
        img_np = np.frombuffer(data, dtype=np.uint8).reshape((height, width, 3))
    elif encoding == image_capnp.Image.Encoding.jpeg or image_capnp.Image.Encoding.png:
        img_np = cv2.imdecode(np.frombuffer(data, dtype=np.uint8), cv2.IMREAD_UNCHANGED)
    else:
        raise ValueError(f"Unsupported image encoding: {encoding}")

    return img_np

def save_camera_frames(args):
    """
    Saves the frames of a particular bag file as png images

    :param args.mcap_path: path of the mcap file
    :param args.img_outdir: output path of the frame images
    """
    mcap_path = args.mcap_path
    img_out = args.img_outdir
    frame_count = int(args.frame_count)

    left_img_path = os.path.join(img_out, "/image_2/")
    right_img_path = os.path.join(img_out, "/image_3/")

    left_img_topic = "S1/stereo1_l"
    right_img_topic = "S1/stereo2_r"

    with open(mcap_path, "rb") as file:
        reader = make_reader(file)
        messages = list(reader.iter_messages())

        print(messages)

        left_img_msgs = [m for m in messages if m.channel.topic == left_img_topic]
        right_img_msgs = [m for m in messages if m.channel.topic == right_img_topic]
        
        count = 0
        i=j=0

        while i < frame_count and j < frame_count:
            if i < len(left_img_msgs):
                left_img = decode_image_msg(left_img_msgs[i].data)
                if left_img != None:
                    cv2.imwrite(os.path.join(left_img_path, f"left_{count:04d}.png"), left_img)
                i += 1    

            if j < len(right_img_msgs):
                right_img = decode_image_msg(right_img_msgs[j].data)
                if right_img != None:
                    cv2.imwrite(os.path.join(right_img_path, f"right_{count:04d}.png"), right_img)
                j += 1
        
        print(f"Extracted {count} image pairs to {img_out}")

        
    


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--mcap_path", help="path of mcap file", default="./input_bags/")
    parser.add_argument("--img_outdir", help="output directory of bag image", default="./input_imgs/")
    #parser.add_argument("--time_offset", help="duration offset (in secs) from start to capture frames from", default=0)
    parser.add_argument("--frame_count", help="number of frames of bag to capture", default=5)


    args = parser.parse_args()

    inspect_mcap(args)
    save_camera_frames(args)

if __name__ == "__main__":
    main()