import glob
import numpy as np
import cv2
import os
import argparse
from pathlib import Path

def calculate_depth(args, fx, baseline):
    images = sorted(glob.glob(args.img_path, recursive=True))[:200]
    output_directory = Path(os.path.dirname(args.img_path))

    for image in images:
        name = Path(image).stem.replace("_disp", "")
    
        disp_np = cv2.imread(image, cv2.IMREAD_UNCHANGED).astype(np.float32)
        print(disp_np)
        disp_np = disp_np * 0.5 #Downscale the disparity by 2 to obtain the real disparity
        disp_np -= 1 #Pixel shift for view correction
        disp_np[disp_np <= 0.0] = 0.1 #Mask all 0 portions to 0.1 to avoid division by 0
        depth_np = (fx * baseline) / disp_np
        depth_np_name = f"{name}_depth.npy"
        depth_np_path = output_directory / depth_np_name
        np.save(depth_np_path, depth_np)

def remove(args):
    files = sorted(glob.glob(args.img_path, recursive=True))[:200]

    for file in files:
        os.remove(file)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("-f", "--img_path", default="./output_stereo/*.png")
    parser.add_argument("-r", "--remove", action="store_true")
    
    args = parser.parse_args()

    fx,baseline = 400, 0.08
    
    if args.remove:
        remove(args)
    calculate_depth(args, fx, baseline)

if __name__ == "__main__":
    main()