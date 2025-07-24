import os
import subprocess

list_to_run = [5, 6, 7, 8, 10, 12]

# Range of frame numbers (adjust start and end as needed)
for i in list_to_run:  # From 7th to 12th
    frame = f"{i}th_frames"
    command1 = [
        "python3", "auto_rectify.py",
        "-r",
        "-f", f"\"./output_stereo/{frame}/*.png\"",
    ]

    command2 = [
        "python3", "generate_stereo.py",
        "-l", f"\"./input_imgs/{frame}/image_2/*.png\"",
        "-r", f"\"./input_imgs/{frame}/image_3/*.png\"",
        "--save_numpy",
        "--no_bag",
        "--output_directory", f"./output_stereo/{frame}"
    ]

    print(f"Running command for {frame}...")
    subprocess.run(" ".join(command2), shell=True, check=True)
