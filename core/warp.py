import torch
import torch.nn.functional as F


def normalize_coords(grid):
    """Normalize coordinates of image scale to [-1, 1]
    Args:
        grid: [B, 2, H, W]
    """
    h, w = grid.size()[2:]
    x = 2 * (grid[:, 0, :, :] / (w - 1)) - 1  # shape: [B, H, W]
    y = 2 * (grid[:, 1, :, :] / (h - 1)) - 1  # shape: [B, H, W]

    grid = torch.stack([x, y], dim=1)  # shape: [B, 2, H, W]
    grid = grid.permute((0, 2, 3, 1))  # [B, H, W, 2]
    return grid


def meshgrid(img, homogeneous=False):
    """Generate meshgrid in image scale
    Args:
        img: [B, _, H, W]
        homogeneous: whether to return homogeneous coordinates
    Return:
        grid: [B, 2, H, W]
    """
    b, _, h, w = img.size()

    x_range = torch.arange(0, w).view(1, 1, w).expand(1, h, w).type_as(img)  # [1, H, W]
    y_range = torch.arange(0, h).view(1, h, 1).expand(1, h, w).type_as(img)

    grid = torch.cat((x_range, y_range), dim=0)  # [2, H, W], grid[:, i, j] = [j, i]
    grid = grid.unsqueeze(0).expand(b, 2, h, w)  # [B, 2, H, W]

    if homogeneous:
        ones = torch.ones_like(x_range).unsqueeze(0).expand(b, 1, h, w)  # [B, 1, H, W]
        grid = torch.cat((grid, ones), dim=1)  # [B, 3, H, W]
    return grid

def interp(x, sample_grid, padding_mode):
    original_dtype = x.dtype
    x_fp32 = x.float()
    sample_grid_fp32 = sample_grid.float()
    with torch.cuda.amp.autocast(enabled=False):
        output_fp32 = F.grid_sample(x_fp32, sample_grid_fp32, mode='bilinear', padding_mode=padding_mode)
    if original_dtype != torch.float32:
        output = output_fp32.to(original_dtype)
    else:
        output = output_fp32
    return output


def disp_warp(img, disp, padding_mode='border'):
    """Warping by disparity
    Args:
        img: [B, 3, H, W]
        disp: [B, 1, H, W], positive
        padding_mode: 'zeros' or 'border'
    Returns:
        warped_img: [B, 3, H, W]
        valid_mask: [B, 3, H, W]
    """

    grid = meshgrid(img)  # [B, 2, H, W] in image scale
    # Note that -disp here
    #Rewritten for model conversion
    B, _, H, W = disp.shape
    device = disp.device
    dtype = disp.dtype

    zero_disp = torch.zeros((B, 1, H, W), device=device, dtype=dtype)
    offset = torch.cat((-disp, zero_disp), dim=1)  #offset = torch.cat((-disp, torch.zeros_like(disp)), dim=1)  # [B, 2, H, W]
    
    sample_grid = grid + offset
    sample_grid = normalize_coords(sample_grid)  # [B, H, W, 2] in [-1, 1]
    # warped_img = F.grid_sample(img, sample_grid, mode='bilinear', padding_mode=padding_mode)
    warped_img = interp(img, sample_grid, padding_mode)


    mask = torch.ones_like(img)
    # valid_mask = F.grid_sample(mask, sample_grid, mode='bilinear', padding_mode='zeros')
    valid_mask = interp(mask, sample_grid, padding_mode)
    valid_mask[valid_mask < 0.9999] = 0
    valid_mask[valid_mask > 0] = 1
    return warped_img, valid_mask