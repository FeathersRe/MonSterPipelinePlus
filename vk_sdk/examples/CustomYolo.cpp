#include "vk_sdk/capnp/Shared.hpp"
#include "vk_sdk/capnp/cameracontrol.capnp.h"
#include <iostream>
#include <memory>
#include <vk_sdk/Sdk.hpp>

/*
    yoloRequest
    < 0 -> manual request mode (per camera), for values less than 0
        Sends a single frame from the selected camera to the Neural Network for inference.
        This is useful for one-time, on-demand inference per camera.

        The mapping for manual selection (negative cam_num) depends on which cameras are enabled for YOLO processing:
          -1 -> Sends one frame from the first enabled YOLO camera (e.g., camb)
          -2 -> Sends one frame from the second enabled YOLO camera (e.g., camc)
          -3 -> Sends one frame from the third enabled YOLO camera (e.g., camd)
          
        Note: Only the specified camera will send its frame when selected.
            
    0 -> disable yolo mode
        once set to "0" the node stops forwarding image frames to the Neural Network.

    > 0 -> auto mode for values more than 0
        sends image frames at a rate of frequency / (num).
*/

int main() {
    auto visualkit = vkc::VisualKit::create(std::nullopt);

    // Check that the object has been created successfully before proceeding.
    if (visualkit == nullptr) {
        std::cout << "Failed to create visualkit connection." << std::endl;
        return -1;
    }

    // Make a capnp object.
    auto builder = std::make_unique<capnp::MallocMessageBuilder>();
    auto yoloRequestBuilder = builder->initRoot<vkc::CameraControl>();

    std::string input;
    auto shared = vkc::Shared<vkc::CameraControl>(std::move(builder));

    // Obtain a receiver from the sink so that we can write to it.
    
    auto yoloRequestReceiver = visualkit->sink().obtain("S0/yolo_request", vkc::Type<vkc::CameraControl>());
    
    // Start the sink for it to start receiving messages.
    visualkit->sink().start();
    std::cout << "press q to exit, else enter framerate" <<std::endl;
    // Call the receiver of the sink to handle the message.
    while(input != "q"){
        std::cin >> input;
        std::cout << "passed: " << input << std::endl;
        yoloRequestBuilder.setYoloRequest(std::stoi(input)); // here we are converting the request from out input into an int. 
        yoloRequestReceiver->handle(shared);
    }
    // Stop the sink. 
    //
    // You may pass `true` here if you want the sink to block until all data from the source has been sent to it.
    //
    // See the documentation for `stop` for more information how to do this correctly.
    visualkit->sink().stop(false);

    return 0;
}