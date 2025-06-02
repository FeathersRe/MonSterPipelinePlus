#include <cmath>
#include <limits>
#include <stdexcept>
#include <iostream>

#include "vk_sdk/DisparityToDistance.hpp"

// #include <spdlog/spdlog.h>

namespace vkc {

    template <typename T>
    T& at(unsigned char* data, uint32_t rows, uint32_t cols, uint32_t row, uint32_t col)
    {
        if (row < 0 || row >= rows || col < 0 || col >= cols)
            throw std::out_of_range("Index out of range");

        T* dataCasted = reinterpret_cast<T*>(data);
        return dataCasted[row * cols + col];
    }

    /**
     * Converts a given disparity image into a Euclidean distance image.
     * 
     * @param disparity The shared capnproto message containing the disparity raw data and metadata.
     * @param euclideanDistConfig The parameters to process the disparity image into a distance image.
     * @return The shared capnproto disparity message containing the Euclidean distance raw data and metadata.
     */
    vkc::Shared<vkc::Disparity> convertToEuclideanDist(vkc::Shared<vkc::Disparity> disparity, const EuclideanDistParams& euclideanDistConfig)
    {
        // Get the disparity image encoding and dimensions
        auto disparityReader = disparity.reader();
        auto disparityHeight = disparityReader.getHeight();
        auto disparityWidth = disparityReader.getWidth();
        auto disparityArray = disparityReader.getData().asBytes();
        auto disparityEncoding = disparityReader.getEncoding();
        auto decimationFactor = disparityReader.getDecimationFactor();
        auto maxDisparity = disparityReader.getMaxDisparity();

        // Get the distance parameters
        auto distanceEncodingParam = euclideanDistConfig.distanceEncoding;
        auto maxDepth = euclideanDistConfig.maxDepth;
        auto disparityOffset = euclideanDistConfig.disparityOffset;
        auto ignoreZeroDisparity = euclideanDistConfig.ignoreZeroDisparity;

        // Extract the disparity data depending on the encoding
        unsigned char* disparityData;

        if (disparityEncoding == vkc::Disparity::Encoding::DISPARITY8)
            disparityData = const_cast<unsigned char*>(disparityReader.getData().asBytes().begin());
        else if (disparityEncoding == vkc::Disparity::Encoding::DISPARITY16)
            disparityData = const_cast<unsigned char*>(disparityReader.getData().asBytes().begin());
        else
            throw std::runtime_error("Encoding of disparity is not supported");

        // Get the focal length and the baseline
        auto Fx = disparityReader.getFx();
        auto baseline = disparityReader.getBaseline();

        // Create the message to populate the distance image
        auto mmb = std::make_unique<capnp::MallocMessageBuilder>();
        vkc::Disparity::Builder msg = mmb->getRoot<vkc::Disparity>();

        vkc::Disparity::Encoding distanceEncoding;
        if (distanceEncodingParam == "distance16")
            distanceEncoding = vkc::Disparity::Encoding::DISTANCE16;
        else if (distanceEncodingParam == "distance32")
            distanceEncoding = vkc::Disparity::Encoding::DISTANCE32;
        else
            throw std::runtime_error("Distance image can only be encoded as distance16 (uint16_t) or distance32 (float32).");
        
        // Set the fields of the distance map
        msg.setHeight(disparityHeight);
        msg.setWidth(disparityWidth);
        msg.setStep(disparityWidth);
        msg.setEncoding(distanceEncoding);
        msg.setMaxDisparity(maxDisparity);
        msg.setBaseline(baseline);

        msg.setCx(disparityReader.getCx());
        msg.setCy(disparityReader.getCy());
        msg.setFx(disparityReader.getFx());
        msg.setFy(disparityReader.getFy());
        msg.setPinholeRotation(disparityReader.getPinholeRotation());
        msg.setHeader(disparityReader.getHeader());
        
        // Allocate the buffer size based on the distance encoding
        unsigned char* distanceData;
        if (distanceEncoding == vkc::Disparity::Encoding::DISTANCE16)
        {   
            msg.initData(msg.getWidth() * msg.getHeight() * 2);
            distanceData = msg.getData().asBytes().begin();
        } 
        else if (distanceEncoding == vkc::Disparity::Encoding::DISTANCE32)
        {
            msg.initData(msg.getWidth() * msg.getHeight() * 4);
            distanceData = msg.getData().asBytes().begin();
        }

        // Iterate through the pixels of the disparity image
        for (uint32_t v = 0; v < disparityHeight; v++)
        {
            for (uint32_t u = 0; u < disparityWidth; u++)
            {   
                // Obtain the disparity value based on the encoding
                float disparity;
                if (disparityEncoding== vkc::Disparity::Encoding::DISPARITY8)
                {
                    disparity = at<uint8_t>(disparityData, disparityHeight, disparityWidth, v, u);
                }
                else if (disparityEncoding== vkc::Disparity::Encoding::DISPARITY16)
                {
                    disparity = at<uint16_t>(disparityData, disparityHeight, disparityWidth, v, u);
                    disparity /= 8.0;
                }

                if (maxDisparity > 950) // Scaling factor when filter is applied (since depthai v2.29.0)
                    disparity /= 10.0;

                // remove injected disparity offset from camera driver
                if (disparity < euclideanDistConfig.disparityOffsetDriver)
                    disparity = 0;
                else
                    disparity -= euclideanDistConfig.disparityOffsetDriver;

                // Calculate the depth at this pixel
                float depth = Fx * baseline / (disparity + disparityOffset);

                // Convert from pixel to camera coordinates
                float u_camera = (u * decimationFactor - disparityReader.getCx()) / disparityReader.getFx() * depth;
                float v_camera = (v * decimationFactor - disparityReader.getCy()) / disparityReader.getFy() * depth;

                // Calculate Euclidean distance
                float distance = static_cast<float>(
                                    std::sqrt(std::pow(u_camera, 2) + std::pow(v_camera, 2) + std::pow(depth, 2)));

                // Filtering of pixels that are invalid
                if (disparityOffset < 0 && distance < 0)
                    distance = std::numeric_limits<float>::quiet_NaN();

                if (disparity == 0 && ignoreZeroDisparity)
                    distance = std::numeric_limits<float>::quiet_NaN();

                if (depth > maxDepth)
                    distance = std::numeric_limits<float>::quiet_NaN();


                // Set the distance value based on the desired distance encoding
                if (distanceEncoding == vkc::Disparity::Encoding::DISTANCE16)
                {   
                    at<uint16_t>(distanceData, disparityHeight, disparityWidth, v, u) = static_cast<uint16_t>(distance * 1000); // mm
                } 
                else if (distanceEncoding == vkc::Disparity::Encoding::DISTANCE32)
                {
                    at<float>(distanceData, disparityHeight, disparityWidth, v, u) = distance; // m
                }         
            }
        }

        return vkc::Shared<vkc::Disparity>(std::move(mmb));
    }
}