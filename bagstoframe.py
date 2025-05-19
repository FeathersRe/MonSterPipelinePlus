import argparse
import os
import capnp
import cv2
import numpy as np
from mcap.reader import make_reader

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
    

def save_camera_frames(args):
    """
    Saves the frames of a particular bag file as png images

    :param args.mcap_path: path of the mcap file
    :param args.img_outdir: output path of the frame images
    """
    mcap_path = args.mcap_path
    img_out = args.img_outdir




def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--mcap_path", help="path of mcap file")
    parser.add_argument("--img_outdir", help="output directory of bag image", default="./input_imgs")

    args = parser.parse_args()

    inspect_mcap(args)
    save_camera_frames(args)

if __name__ == "__main__":
    main()