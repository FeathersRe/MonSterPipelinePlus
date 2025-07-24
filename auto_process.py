import os
import re
import argparse

def sync_and_remove(args):
    if os.path.exists(args.frame_path):
        left_dir = os.path.join(args.frame_path, "image_2")
        right_dir = os.path.join(args.frame_path, "image_3")

    pattern = re.compile(r"left_(\d+)\.png") 

    left_pair = set()
    for filename in os.listdir(left_dir):
        match = pattern.match(filename)
        if match:
            left_pair.add(match.group(1))

    for filename in os.listdir(right_dir):
        match = re.match(r'right_(\d+)\.png', filename) 
        if match:
            xxxx = match.group(1)
            if xxxx not in left_pair:
                file_to_delete = os.path.join(right_dir, filename)
                os.remove(file_to_delete)

def auto_rename(args):
    if os.path.exists(args.frame_path):
        left_dir = os.path.join(args.frame_path, "image_2")
        right_dir = os.path.join(args.frame_path, "image_3")
    
    if len(os.listdir(left_dir)) != len(os.listdir(right_dir)):
        print("Different file count, ABORT.")
        return

    names = []
    for filename in os.listdir(left_dir):
        names.append(filename)
    
    
    def sort_key(filename):
        match = re.match(r'(\d+)m(\d+)\.png', filename)
        if match:
            first_num = int(match[1])
            second_num = int(match[2])
            priority = 0 if first_num < 10 else 1
            return (priority, first_num, second_num)
        return (2, 0, 0)  # fallback for unexpected format

    names = sorted(names, key=sort_key)

    count = 0
    for file in os.listdir(right_dir):
        src = os.path.join(right_dir, file)
        dst = os.path.join(right_dir, names[count])
        os.rename(src, dst)
        count+=1    

def auto_name(args):

    sync_and_remove(args)

    meter_prefix = 1
    picture_no = 1

    if os.path.exists(args.frame_path):
        left_dir = os.path.join(args.frame_path, "image_2")
    
    for filename in os.listdir(left_dir):
        src = os.path.join(left_dir, filename)
        new_name = str(meter_prefix)+"m"+str(picture_no)+".png"
        dst = os.path.join(left_dir, new_name)
        os.rename(src,dst)

        picture_no += 1
        if picture_no == 3:
            meter_prefix+=1
            picture_no = 1
    
    auto_rename(args)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--frame_path", default="./input_imgs/")
    parser.add_argument("-r", "--remove", help="sync and remove extra image pairs", action="store_true")
    parser.add_argument("-a", "--autoname", action="store_true")
    parser.add_argument("-n", "--rename", help="sync and rename image pairs", action="store_true")

    args = parser.parse_args()
    if args.remove:
        sync_and_remove(args)
    if args.autoname:
        auto_name(args)
    if args.rename:
        auto_rename(args)

if __name__ == "__main__":
    main()