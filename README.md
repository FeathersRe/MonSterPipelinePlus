# 🚀MonSter Pipeline (Cloud Conversion)

## Abstract
The [MonSter](https://github.com/Junda24/MonSter/tree/main) disparity estimation model presents a novel approach of combining both monocular estimation and stereo matching methods in estimating image disparity. It takes in a set of stereo pair images as input, utilise [Depth AnythingV2](https://github.com/DepthAnything/Depth-Anything-V2)'s DINO V2 encoder and DPT encoder to generate monocular estimations, then enter a continuous stereo and mono guided refinement process to output an end disparity. 

This repository abstracts the base modules for disparity generation from the various source repositories, serving the purpose of pipelining the depth estimation process from images obtained from the VK180.

## Requirements
* Docker
* VSC: DevContainers
* MonSter ([*mix_all.pth*](https://huggingface.co/cjd24/MonSter/resolve/main/mix_all.pth?download=true))
* Depth AnythingV2 (currently only supports [*depth_anything_v2_vitl.pth*](https://huggingface.co/depth-anything/Depth-Anything-V2-Large/resolve/main/depth_anything_v2_vitl.pth?download=true))
* vk_sdk (from vk-systems)

## Set-up
1. Build image with dockerfile and run container
```Shell
#Building image file
docker build -t monster-pipeline-env ./docker/
```
> In VSC: With Project Directory Opened: F1 > Dev Containers: Reopen in Container
 
2. Load MonSter and Depth AnythingV2 models with pretrained weights
```Shell
#Recommended directory structure for load
├── auto_record.py
├── bagstoframe.py
├── core #MonSter Core
├── Depth-Anything-V2-list3 #Depth Anything V2 Core
├── vk_sdk #vk_sdk from vksystems
├── docker 
│   ├── Dockerfile
│   └── requirements.txt
├── generate_stereo.py
├── pretrained
│   ├── depth_anything_v2_vitl.pth #Depth Anything V2 weights
│   └── mix_all.pth #MonSter weights
└── visualise_metric_st.py
```

## Loading target bags
A bag file can be obtained using *auto_record.py* or directly loaded to */input_bags*
```Shell
#Pipeline bag recording with auto_record.py
python3 auto_record.py -d <recording_duration>
```

## Obtaining disparity and depths
The disparity heatmap can be directly obtained by running generate_stereo.py
```Shell
#Generate disparity heatmaps for bag at mcap_path
python3 generate_stereo.py --mcap_path <path to mcap file>
```

To save depth information as .npy computed from disparity for further visualisation, run
```Shell
#Saving depth .npy data for depth visualisation
python3 generate_stereo.py --mcap_path <path to mcap file> --save_numpy
```

By default, generate stereo wll utilise stereo1_l and stereo1_r pairs for disparity estimation. If the mcap contains two sets of stereo, stereo2 can be specified with
```Shell
python3 generate_stereo.py --mcap_path <path to mcap file> --stereo_2
```

## Visualisation
To visualise the depth data generated, run
```Shell
streamlit run visualise_metric_st.py
```
A streamlit instance will then be launched providing depth data visualisation.

## Cloud Operations
For this deviation of MonSter, cloud improvements were introduced to enable its deployment on the Qualcomm AI Hub

### Model Tracing
The *model_tracing.py* script traces the MonSter model into the .onnx format. To have it working, simply prepare one set of left and right images for tracing, then run
```Shell
> python3 model_tracing.py
```

### Model Valdation and Deployment to Cloud
* The *model_validation.py* script validates the .onnx format model generated. The script can be executed in a similar manner to *generate_stereo.py*.
* For the deployment of the model to the Qualcomm AI Cloud, use *cloud_validation.py*. Ater having your qai_token configured in qai_hub, simply run:
```Shell
> python3 cloud_validation.py
```

This executes the cloud pipelines and uploads the model to the ai hub to generate inference statistics.

A backup of the artifacts generated (.onnx, .dlc models) can be found in the root folder of the HDD, under /Archives