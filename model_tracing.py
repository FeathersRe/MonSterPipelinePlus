import argparse
import glob
import numpy as np
import torch
from tqdm import tqdm
from pathlib import Path
from core.monster import Monster
from wrapper import ModelWrapper

from core.utils.utils import InputPadder
from PIL import Image
from matplotlib import pyplot as plt
import os 
import cv2
import torch.nn.functional as F
import sys
import time

from pathlib import Path

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

def generate_stereo(args):
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

    imfile1 = sorted(glob.glob(args.left_imgs, recursive=True))[:200][0]
    imfile2 = sorted(glob.glob(args.right_imgs, recursive=True))[:200][0]

    image1 = load_image(imfile1)
    image2 = load_image(imfile2)

    padder = InputPadder(image1.shape, divis_by=32)
    image1, image2 = padder.pad(image1, image2)
    
    model = ModelWrapper(model, args.valid_iters)

    torch.onnx.export(
        model,
        (image1, image2),               # Pass as a tuple
        "monster_traced_model.onnx",
        export_params=True,
        opset_version=16,
        do_constant_folding=True,
        input_names=['image1', 'image2'],           # Name both inputs
        output_names=['output'],                 # Output name
        dynamic_axes={
            'image1': {0: 'batch_size'},
            'image2': {0: 'batch_size'},
            'output': {0: 'batch_size'}
        }
    )


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

    args= parser.parse_args()
    generate_stereo(args)

if __name__ == "__main__":
    main()