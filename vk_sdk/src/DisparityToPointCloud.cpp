#include <iostream>
#include <vector>
#include <cmath>
#include <limits>
#include <stdexcept>

#include "vk_sdk/DisparityToPointCloud.hpp"

namespace vkc {

    template <typename T>
    T& at(unsigned char* data, uint32_t rows, uint32_t cols, uint32_t row, uint32_t col)
    {
        if (row < 0 || row >= rows || col < 0 || col >= cols)
            throw std::out_of_range("Index out of range");

        T* dataCasted = reinterpret_cast<T*>(data);
        return dataCasted[row * cols + col];
    }

    vkc::Shared<vkc::PointCloud> convertToPointCloud(vkc::Shared<vkc::Disparity> disparity, 
                                                     unsigned char* image, 
                                                     const PointCloudParams& pcConfig,
                                                     int skip_pixel)
    {
        // Get the disparity image encoding and dimensions
        auto disparityReader = disparity.reader();
        auto disparityEncoding = disparityReader.getEncoding();
        auto height = disparityReader.getHeight();
        auto width = disparityReader.getWidth();
        auto maxDisparity = disparityReader.getMaxDisparity();

        // Get the disparity parameters
        auto maxDepth = pcConfig.maxDepth;
        auto disparityOffset = pcConfig.disparityOffset;

        // Get the focal length, optical center, baseline, and decimation factor for depth calculation
        auto Fx = disparityReader.getFx();
        auto Cx = disparityReader.getCx();
        auto Cy = disparityReader.getCy();
        auto baseline = disparityReader.getBaseline();
        auto decimationFactor = disparityReader.getDecimationFactor();

        // Extract the disparity data depending on the encoding
        unsigned char* disparityData;

        if (disparityEncoding == vkc::Disparity::Encoding::DISPARITY8)
            disparityData = const_cast<unsigned char*>(disparityReader.getData().asBytes().begin());
        else if (disparityEncoding == vkc::Disparity::Encoding::DISPARITY16)
            disparityData = const_cast<unsigned char*>(disparityReader.getData().asBytes().begin());
        else
            throw std::runtime_error("Encoding of disparity is not supported");
        
        // Create the message to populate the point cloud
        auto mmb = std::make_unique<capnp::MallocMessageBuilder>();
        vkc::PointCloud::Builder msg = mmb->getRoot<vkc::PointCloud>();

        msg.setHeader(disparityReader.getHeader());
        
        // Set point stride of 15 bytes (4 each for XYZ and 1 each for RGB)
        int pointBytes = 15;
        msg.setPointStride(pointBytes);

        // The fields are X, Y, Z, R, G, B
        auto fields = msg.initFields(6);

        // X
        fields[0].setName("x");
        fields[0].setOffset(0);
        fields[0].setType(vkc::Field::NumericType::FLOAT32);

        // Y
        fields[1].setName("y");
        fields[1].setOffset(4);
        fields[1].setType(vkc::Field::NumericType::FLOAT32);

        // Z
        fields[2].setName("z");
        fields[2].setOffset(8);
        fields[2].setType(vkc::Field::NumericType::FLOAT32);

        // R
        fields[3].setName("r");
        fields[3].setOffset(12);
        fields[3].setType(vkc::Field::NumericType::UINT8);

        // G
        fields[4].setName("g");
        fields[4].setOffset(13);
        fields[4].setType(vkc::Field::NumericType::UINT8);

        // B
        fields[5].setName("b");
        fields[5].setOffset(14);
        fields[5].setType(vkc::Field::NumericType::UINT8);

        unsigned char* pcData;
        int num_points = width * height / (skip_pixel * skip_pixel);
        msg.initPoints(num_points * pointBytes);
        pcData = msg.getPoints().asBytes().begin();
        int invalidCount = 0;

        // std::cout << "height: " << height << "width: " << width << std::endl;


        for (uint32_t v = 0; v < height; v+=skip_pixel)
        {
            for (uint32_t u = 0; u < width; u+=skip_pixel)
            {
                float disparity;
                if (disparityEncoding == vkc::Disparity::Encoding::DISPARITY8)
                {
                    disparity = at<uint8_t>(disparityData, height, width, v, u);
                }
                else if (disparityEncoding == vkc::Disparity::Encoding::DISPARITY16)
                {
                    disparity = at<uint16_t>(disparityData, height, width, v, u);
                    disparity /= 8.0;
                }

                if (maxDisparity > 950) // Scaling factor when filter is applied (since depthai v2.29.0)
                    disparity /= 10.0;

                // remove injected disparity offset from camera driver
                if (disparity < pcConfig.disparityOffsetDriver)
                    disparity = 0;
                else
                    disparity -= pcConfig.disparityOffsetDriver;

                // Calculate the depth value of the point
                float depth = Fx * baseline / (disparity + disparityOffset);

                // Exclude from point cloud if disparity is too low or if depth is too high
                if (disparity < 5 || depth >= maxDepth){
                    invalidCount++;
                    continue;
                }

                // Calculate the point coordinates while accounting for the decimation factor
                float pt_x = (u * decimationFactor - Cx) * depth / Fx;
                float pt_y = (v * decimationFactor - Cy) * depth / Fx;
                float pt_z = depth;
                    
                // Get the rgb values of that image pixel
                int index = (v * decimationFactor * width * decimationFactor + u * decimationFactor) * 3;

                uint8_t b = image[index];
                uint8_t g = image[index + 1];
                uint8_t r = image[index + 2];
                
                // Set the fields of the point
                *reinterpret_cast<float*>(pcData) = pt_x;
                *reinterpret_cast<float*>(pcData + 4) = pt_y;
                *reinterpret_cast<float*>(pcData + 8) = pt_z;

                *(pcData+12) = r;
                *(pcData+13) = g;
                *(pcData+14) = b;
                
                // Shift the pointer to the next point
                pcData += pointBytes;
            }
        }

        // Truncate point cloud buffer to only store valid points
        auto validBytes = static_cast<capnp::uint>((num_points - invalidCount) * pointBytes);
        auto orphan = msg.disownPoints();
        orphan.truncate(validBytes);
        msg.adoptPoints(kj::mv(orphan));

        return vkc::Shared<vkc::PointCloud>(std::move(mmb));
    }
}