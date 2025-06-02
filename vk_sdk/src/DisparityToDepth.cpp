#include <cmath>
#include <limits>
#include <stdexcept>
#include <iostream>

#include "vk_sdk/DisparityToDepth.hpp"

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
     * Converts a given disparity image into a depth image.
     * 
     * @param disparity The shared capnproto message containing the disparity raw data and metadata.
     * @param depthConfig The parameters to process the disparity image into a depth image.
     * @return The shared capnproto disparity message containing the depth raw data and metadata.
     */
    vkc::Shared<vkc::Disparity> convertToDepth(vkc::Shared<vkc::Disparity> disparity, const DepthParams& depthConfig)
    {
        // Get the disparity image encoding and dimensions
        auto disparityReader = disparity.reader();
        auto disparityHeight = disparityReader.getHeight();
        auto disparityWidth = disparityReader.getWidth();
        auto disparityArray = disparityReader.getData().asBytes();
        auto disparityEncoding = disparityReader.getEncoding();
        auto maxDisparity = disparityReader.getMaxDisparity();

        // Get the depth parameters
        auto depthEncodingParam = depthConfig.depthEncoding;
        auto maxDepth = depthConfig.maxDepth;
        auto disparityOffset = depthConfig.disparityOffset;
        auto ignoreZeroDisparity = depthConfig.ignoreZeroDisparity;

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

        // Create the message to populate the depth image
        auto mmb = std::make_unique<capnp::MallocMessageBuilder>();
        vkc::Disparity::Builder msg = mmb->getRoot<vkc::Disparity>();

        vkc::Disparity::Encoding depthEncoding;
        if (depthEncodingParam == "depth16")
            depthEncoding = vkc::Disparity::Encoding::DEPTH16;
        else if (depthEncodingParam == "depth32")
            depthEncoding = vkc::Disparity::Encoding::DEPTH32;
        else
            throw std::runtime_error("Depth image can only be encoded as depth16 (uint16_t) or depth32 (float32).");
        
        // Set the fields of the depth map
        msg.setHeight(disparityHeight);
        msg.setWidth(disparityWidth);
        msg.setStep(disparityWidth);
        msg.setEncoding(depthEncoding);
        msg.setMaxDisparity(maxDisparity);
        msg.setBaseline(baseline);

        msg.setCx(disparityReader.getCx());
        msg.setCy(disparityReader.getCy());
        msg.setFx(disparityReader.getFx());
        msg.setFy(disparityReader.getFy());
        msg.setPinholeRotation(disparityReader.getPinholeRotation());
        msg.setHeader(disparityReader.getHeader());
        
        // Allocate the buffer size based on the depth encoding
        unsigned char* depthData;
        if (depthEncoding == vkc::Disparity::Encoding::DEPTH16)
        {   
            msg.initData(msg.getWidth() * msg.getHeight() * 2);
            depthData = msg.getData().asBytes().begin();
        } 
        else if (depthEncoding == vkc::Disparity::Encoding::DEPTH32)
        {
            msg.initData(msg.getWidth() * msg.getHeight() * 4);
            depthData = msg.getData().asBytes().begin();
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
                if (disparity < depthConfig.disparityOffsetDriver)
                    disparity = 0;
                else
                    disparity -= depthConfig.disparityOffsetDriver;

                // Calculate the depth at this pixel
                float depth = Fx * baseline / (disparity + disparityOffset);

                // Filtering of pixels that are invalid
                if (disparityOffset < 0 && depth < 0)
                    depth = std::numeric_limits<float>::quiet_NaN();

                if (disparity == 0 && ignoreZeroDisparity)
                    depth = std::numeric_limits<float>::quiet_NaN();

                if (depth > maxDepth)
                    depth = std::numeric_limits<float>::quiet_NaN();

                // Set the depth value based on the desired depth encoding
                if (depthEncoding == vkc::Disparity::Encoding::DEPTH16)
                {   
                    at<uint16_t>(depthData, disparityHeight, disparityWidth, v, u) = static_cast<uint16_t>(depth * 1000);
                } 
                else if (depthEncoding == vkc::Disparity::Encoding::DEPTH32)
                {
                    at<float>(depthData, disparityHeight, disparityWidth, v, u) = depth;
                } 
            }
        }

        return vkc::Shared<vkc::Disparity>(std::move(mmb));
    }
}