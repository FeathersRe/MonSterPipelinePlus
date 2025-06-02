import os
import time
import signal
import subprocess
import argparse
from datetime import datetime

def vk180_record(args):
    timestamp = datetime.now().strftime("%m-%d_%H-%M-%S")
    filename = f"recording_{timestamp}.mcap"
    output_path = os.path.join(args.output_dir, filename)

    camdriver_command = ['vk_camera_driver',
                         args.camera_config]

    record_command = ['vk_record',
                      output_path,
                      '-f',
                      args.record_config]
    
    cam_process = subprocess.Popen(camdriver_command)
    print("[INFO] cam_process started, waiting for setup...")

    # Wait for the camera to initialize (setup time)
    time.sleep(10)

    # Start the recording process
    record_process = subprocess.Popen(record_command)
    print("[INFO] record_process started.")

    # Let the recording run for the specified duration
    time.sleep(float(args.recording_dur))

    # Send SIGINT (like Ctrl+C) to gracefully stop recording
    print("[INFO] Sending SIGINT to record_process...")
    record_process.send_signal(signal.SIGINT)

    # Wait for record_process to finish saving and terminate
    record_process.wait()
    print("[INFO] record_process finished saving and exited.")

    # Now terminate the camera process
    cam_process.send_signal(signal.SIGINT)
    cam_process.terminate()
    print("[INFO] cam_process terminated.")

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-c', '--camera_config', help="path of camera driver config", default="/media/jingyu/D/ProjectCodes/May/vk-system/install/configs/camera_driver/vk180_light_rectified.json")
    parser.add_argument('-r', '--record_config', help="path of record driver config", default="/media/jingyu/D/ProjectCodes/May/vk-system/install/configs/tools/record/VK180.json")
    parser.add_argument('-o', '--output_dir', help="output directory of mcap file", default="./input_bags")
    parser.add_argument('-d', '--recording_dur', help="time duration of recording (in secs)", default=30)
    
    args = parser.parse_args()
    vk180_record(args)

if __name__ == "__main__":
    main()