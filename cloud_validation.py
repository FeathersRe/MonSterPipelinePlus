import qai_hub as hub
import torch
import glob
import argparse
import numpy as np
from PIL import Image
import os

from core.monster import Monster
from core.utils.utils import InputPadder
from pipeline_utils import trace_model

DEVICE = "cpu"

os.environ['CUDA_VISIBLE_DEVICES'] = '0'

def upload_cloud(args):
    #traced_model = trace_model(args)
    traced_model = "./monster_traced_model.onnx"

    #Compile Model
    compile_job = hub.submit_compile_job(
        model=traced_model,
        device=hub.Device("QCS9075 (Proxy)"),
        input_specs={
            "image1": (1,3,416,640),
            "image2": (1,3,416,640),
        },
    )

    target_model = compile_job.get_target_model()
    profile_job = hub.submit_profile_job(
        model=target_model,
        device=hub.Device("QCS9075 (Proxy)")
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
    upload_cloud(args)

if __name__ == "__main__":
    main()
