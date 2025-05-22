import argparse
import glob
import numpy as np
import torch
from tqdm import tqdm
from pathlib import Path
from core.monster import Monster

from core.utils.utils import InputPadder
from PIL import Image
from matplotlib import pyplot as plt
import os 
import cv2
import torch.nn.functional as F
import sys
import time

from pathlib import Path
from bagstoframe import inspect_mcap, save_camera_frames, save_camera_specs

DEVICE = 'cpu'

os.environ['CUDA_VISIBLE_DEVICES'] = '0'

class NormalizeTensor(object):
    """
    Normalize a tensor by given mean and std.
    """
    
    def __init__(self, mean, std):
        self.mean = torch.tensor(mean)
        self.std = torch.tensor(std)

    def __call__(self, tensor):
        Device = tensor.device
        self.mean = self.mean.to(Device)
        self.std = self.std.to(Device)
    
        if self.mean.ndimension() == 1:
            self.mean = self.mean[:,None, None]
        if self.std.ndimension() == 1:
            self.std = self.std[:, None, None]
        
        return (tensor - self.mean) / self.std

def load_image(imfile):
    img = np.array(Image.open(imfile)).astype(np.uint8)
    img = torch.from_numpy(img).permute(2, 0, 1).float()
    return img[None].to(DEVICE)

def generate_stereo(args, fx, baseline):
    model = torch.nn.DataParallel(Monster(args), device_ids=[0])

    checkpoint = torch.load(args.restore_ckpt, map_location=torch.device("cpu"))
    ckpt = dict()
    
    if 'state_dict' in checkpoint.keys():
        checkpoint = checkpoint['state_dict']
    
    for key in checkpoint:
        if key.startswith("module."):
            ckpt[key] = checkpoint[key]
        else:
            ckpt["module." + key] = checkpoint[key]
    
    model.load_state_dict(ckpt, strict=True)

    model = model.module
    model.to(DEVICE)
    model.eval()
    output_directory = Path(args.output_directory)
    output_directory.mkdir(exist_ok=True)

    mean = [0.485, 0.456, 0.406]
    std = [0.229, 0.224, 0.225]
    normalize = NormalizeTensor(mean, std)

    with torch.no_grad():
        left_images = sorted(glob.glob(args.left_imgs, recursive=True))[:200]
        right_images = sorted(glob.glob(args.right_imgs, recursive=True))[:200]
        print (f"Found {len(left_images)} images. Saving files to {output_directory}/")

        for (imfile1, imfile2) in tqdm(list(zip(left_images, right_images))):
            image1 = load_image(imfile1)
            image2 = load_image(imfile2)
            padder = InputPadder(image1.shape, divis_by=32)
            image1, image2 = padder.pad(image1, image2)
            start_time = time.time()
            disp = model(image1, image2, iters=args.valid_iters, test_mode=True)  

            end_time = time.time()
            inference_time = end_time - start_time
            print(f"Inference time: {inference_time:.4f} seconds")
            disp = padder.unpad(disp)
            file_stem = os.path.join(output_directory, imfile1.split('/')[-1]).replace('.png', '')
            disp = disp.cpu().numpy().squeeze()
            disp_np = (2.0*disp).astype(np.uint8) #Grey colourmap
            
            print(disp_np.shape)

            colour_disp_np = cv2.applyColorMap(disp_np, cv2.COLORMAP_PLASMA)
            left_name = Path(imfile1).stem  # e.g., '000001'
            disp_img_name = f"{left_name}_disp.png"
            disp_img_path = output_directory / disp_img_name
            cv2.imwrite(str(disp_img_path), colour_disp_np)

            #Package into seperate script to avoid model computation
            #disp_np = cv2.imread('disparity.png', cv2.IMREAD_UNCHANGED).astype(np.float32)
            disp_np[disp_np == 0.0] = 0.1 #Mask all 0 portions to 0.1 to avoid division by 0
            depth_np = (2 * fx * baseline) / disp_np
            depth_np_name = f"{left_name}_depth.npy"
            depth_np_path = output_directory / depth_np_name
            np.save(depth_np_path, depth_np)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--restore_ckpt', help="restore checkpoint", default="./pretrained/mix_all.pth")

    parser.add_argument('--save_numpy', action='store_true', help='save output as numpy arrays')

    parser.add_argument('-l', '--left_imgs', help="path to all first (left) frames", default="./input_imgs/image_2/*.png")
    parser.add_argument('-r', '--right_imgs', help="path to all second (right) frames", default="./input_imgs/image_3/*.png")

    parser.add_argument('--output_directory', help="directory to save stereo output", default="./output_stereo")
    parser.add_argument('--mixed_precision', action='store_true', help='use mixed precision')
    parser.add_argument('--valid_iters', type=int, default=16, help='number of flow-field updates during forward pass')
    parser.add_argument('--encoder', type=str, default='vitl', choices=['vits', 'vitb', 'vitl', 'vitg'])

    parser.add_argument('--stereo_unit', help="Defines the stereo unit that is used for original image", default = 1)

    # Architecture choices
    parser.add_argument('--hidden_dims', nargs='+', type=int, default=[128]*3, help="hidden state and context dimensions")
    parser.add_argument('--corr_implementation', choices=["reg", "alt", "reg_cuda", "alt_cuda"], default="reg", help="correlation volume implementation")
    parser.add_argument('--shared_backbone', action='store_true', help="use a single backbone for the context and feature encoders")
    parser.add_argument('--corr_levels', type=int, default=2, help="number of levels in the correlation pyramid")
    parser.add_argument('--corr_radius', type=int, default=4, help="width of the correlation pyramid")
    parser.add_argument('--n_downsample', type=int, default=2, help="resolution of the disparity field (1/2^K)")
    parser.add_argument('--slow_fast_gru', action='store_true', help="iterate the low-res GRUs more frequently")
    parser.add_argument('--n_gru_layers', type=int, default=3, help="number of hidden GRU levels")
    parser.add_argument('--max_disp', type=int, default=192, help="max disp of geometry encoding volume")

    # Bags to frame settings
    parser.add_argument("--mcap_path", help="path of mcap file", default="./input_bags/")
    parser.add_argument("--img_outdir", help="output directory of bag image", default="./input_imgs/")
    parser.add_argument("--time_offset", help="duration offset (in secs) from start to capture frames from", default=0)
    parser.add_argument("--time_step", help="time step (in s) between the bag frames captured", default=1)
    parser.add_argument("--frame_count", help="number of frames of bag to capture", default=5)

    args= parser.parse_args()
    inspect_mcap(args)
    save_camera_frames(args)
    fx1, baseline1, fx2, baseline2 = save_camera_specs(args)
    if args.stereo_unit == 1:
        generate_stereo(args, fx1, baseline1)
    else:
        generate_stereo(args, fx2, baseline2)

if __name__ == "__main__":
    main()