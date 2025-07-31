from PIL import Image
import glob
import numpy as np
import torch

from core.monster import Monster
from core.utils.utils import InputPadder

DEVICE = "cpu"

def load_image(imfile):
    img = np.array(Image.open(imfile)).astype(np.uint8)
    img = torch.from_numpy(img).permute(2, 0, 1).float()
    return img[None].to(DEVICE)

def trace_model(args):
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

    traced_model = torch.jit.trace(model, (image1,image2))
    return traced_model