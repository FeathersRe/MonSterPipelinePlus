
#include <atomic>
#include <capnp/serialize.h>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <ecal/ecal_time.h>
#include <exception>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>

#include <kj/io.h>
#include <ecal/ecal_core.h>
#include <ecal/ecal_subscriber.h>
#include <ecal/ecal_publisher.h>
#include <thread>
#include <IXWebSocket.h>
#include <IXNetSystem.h>

#include "system.capnp.h"
#include "vk_sdk/Data.hpp"
#include "vk_sdk/Logging.hpp"
#include "vk_sdk/Message.hpp"
#include "vk_sdk/Receivers.hpp"
#include "vk_sdk/VisualKit.hpp"

namespace vkc {
    // Data endpoint's capturing state.
    struct FiltererState {
        enum class Mode {
            Capturing,
            Filtering,
            Terminated,
        };

        std::atomic<Mode> mMode = Mode::Filtering;
        std::mutex mActiveReceiverCountMutex;
        std::condition_variable mActiveReceiverCountCV;
        size_t mActiveReceiverCount = 0;

        void incrementReceiverCount() {
            std::lock_guard lock(mActiveReceiverCountMutex);
            mActiveReceiverCount += 1;
        }

        void decrementReceiverCount() {
            {
                std::lock_guard lock(mActiveReceiverCountMutex);
                mActiveReceiverCount -= 1;
            }
            mActiveReceiverCountCV.notify_one();
        }

        void waitTillNoReceivers() {
            std::unique_lock lock(mActiveReceiverCountMutex);
            while (mActiveReceiverCount > 0) {
                mActiveReceiverCountCV.wait(lock);
            }
        }
    };

    // Wrapper class to help manage a data endpoint's receivers based on its capturing state.
    template<typename T>
    struct FilteringReceiver : Receiver<T> {
    public:
        FilteringReceiver(std::unique_ptr<Receiver<T>> inner, std::shared_ptr<FiltererState> state)
            : mInnerReceiver(std::move(inner)),
              mFiltererState(std::move(state))
        {
            mFiltererState->incrementReceiverCount();
        }
        FilteringReceiver(FilteringReceiver<T>&& other) {
            mInnerReceiver = std::move(other.mInnerReceiver);
            mFiltererState = std::move(other.mFiltererState);
        }

        ~FilteringReceiver() {
            if (mFiltererState) {
                mFiltererState->decrementReceiverCount();
            }
        }

        ReceiverStatus handle(const Message<Shared<T>>& message) override {
            switch (mFiltererState->mMode.load()) {
            case FiltererState::Mode::Capturing:
                return mInnerReceiver->handle(message);
            case FiltererState::Mode::Filtering:
                return ReceiverStatus::Open;
            case FiltererState::Mode::Terminated:
                return ReceiverStatus::Closed;
            default:
                return ReceiverStatus::Open; // Should be unreachable.
            };
        }

        std::unique_ptr<Receiver<T>> mInnerReceiver;
        std::shared_ptr<FiltererState> mFiltererState;
    };

    struct VisualKitWebSocketReceivers {
        std::mutex mMutex;
        std::map<ReceiverId, FilteringReceiver<ManagerMessage>> mReceivers;
    };

    struct VisualKitWebSocket {
        void run(std::string_view host, std::shared_ptr<VisualKitWebSocketReceivers> receivers) {
            auto counter = std::make_shared<std::atomic<uint64_t>>(0);

            mSocket.setUrl("ws://" + std::string(host) + "/socket");
            mSocket.setOnMessageCallback([counter, receivers](const ix::WebSocketMessagePtr& webSocketMessage) {
                if (webSocketMessage->type == ix::WebSocketMessageType::Message) {
                    if (webSocketMessage->binary) {
                        try {
                            auto data = webSocketMessage->str.data();
                            auto dataSize = webSocketMessage->str.size();
                            kj::ArrayPtr<const kj::byte> bytes(reinterpret_cast<const kj::byte*>(data), dataSize);
                            kj::ArrayInputStream stream(bytes);
                            Shared<ManagerMessage> payload(stream);

                            Message<Shared<ManagerMessage>> message;
                            message.payload = payload;
                            message.metadata.publishTime = eCAL::Time::GetMicroSeconds();
                            message.metadata.sequenceNumber = counter->fetch_add(1);

                            std::lock_guard lock(receivers->mMutex);
                            for (auto& [_, receiver] : receivers->mReceivers) {
                                receiver.handle(message);
                            }
                        } catch (std::exception& exception) {
                            log(LogLevel::WARN, exception.what());
                        }
                    }
                } else if (webSocketMessage->type == ix::WebSocketMessageType::Error) {
                    log(LogLevel::WARN, webSocketMessage->errorInfo.reason);
                }
            });
            mSocket.connect(5000);
            mSocket.start();
        }

        std::mutex mMutex;
        ix::WebSocket mSocket;
    };

    class VisualKitSource: public DataSource {
    public:
        VisualKitSource(std::shared_ptr<VisualKitWebSocketReceivers> receivers) : mWebSocketReceivers(receivers) {
            mState = std::make_shared<FiltererState>();
        }

        ~VisualKitSource() {
            stop(false);
            mState->mMode = FiltererState::Mode::Terminated;
        }

        void start() override {
            mState->mMode = FiltererState::Mode::Capturing;
        }

        void stop(bool waitForExhaustion) override {
            if (waitForExhaustion) {
                std::this_thread::sleep_for(std::chrono::seconds(INT64_MAX));
            }

            mState->mMode = FiltererState::Mode::Filtering;
        }

        template<typename T>
        inline ReceiverId installEcalReceiver(std::string_view topic, std::string_view topicType, std::unique_ptr<Receiver<T>> receiver) {
            eCAL::CSubscriber subscriber((std::string(topic)), std::string(topicType));

            if (!subscriber.IsCreated()) {
                return ReceiverId::invalid();
            }

            auto callback = [
                topic = std::string(topic),
                receiver = std::shared_ptr(std::move(receiver)),
                state = mState
            ](const char*, const eCAL::SReceiveCallbackData* data) mutable {
                if (state->mMode.load() != FiltererState::Mode::Capturing) return;
                if (receiver == nullptr) return;

                try {
                    kj::ArrayPtr<const kj::byte> bytes(reinterpret_cast<const kj::byte*>(data->buf), data->size);
                    kj::ArrayInputStream stream(bytes);
                    Shared<T> payload(stream);

                    Metadata metadata;
                    metadata.publishTime = static_cast<uint64_t>(data->time);

                    Message<Shared<T>> message(payload, metadata);

                    if (receiver->handle(topic, message) == ReceiverStatus::Closed) {
                        receiver = nullptr;
                    }
                } catch (std::exception& exception) {
                    log(LogLevel::WARN, exception.what());
                } catch (...) {
                    std::string message;
                    message.append("Unknown exception caught by data source (topic \"");
                    message.append(topic);
                    message.append("\").");

                    log(LogLevel::WARN, message);
                }
            };

            if (!subscriber.AddReceiveCallback(std::move(callback))) {
                return ReceiverId::invalid();
            }

            auto receiverId = ++mNextReceiverId;
            mEcalSubscribers.emplace(ReceiverId(receiverId), std::move(subscriber));
            return receiverId;
        }

        ReceiverId install(std::string_view topic, std::unique_ptr<Receiver<ManagerMessage>> receiver) override {
            if (topic == "ws/message") {
                auto receiverId = ++mNextReceiverId;
                auto filteringReceiver = FilteringReceiver<ManagerMessage>(std::move(receiver), mState);
                std::lock_guard lock(mWebSocketReceivers->mMutex);
                mWebSocketReceivers->mReceivers.emplace(ReceiverId(receiverId), std::move(filteringReceiver));
                return receiverId;
            } else {
                return ReceiverId::invalid();
            }
        }
        ReceiverId install(std::string_view topic, std::unique_ptr<Receiver<Image>> receiver) override {
            return installEcalReceiver<Image>(topic, "capnp:Image", std::move(receiver));
        }
        ReceiverId install(std::string_view topic, std::unique_ptr<Receiver<Imu>> receiver) override {
            return installEcalReceiver<Imu>(topic, "capnp:Imu", std::move(receiver));
        }

        ReceiverId install(std::string_view topic, std::unique_ptr<Receiver<GPS>> receiver) override {
            return installEcalReceiver<GPS>(topic, "capnp:GPS", std::move(receiver));
        }

        ReceiverId install(std::string_view topic, std::unique_ptr<Receiver<ImuList>> receiver) override {
            return installEcalReceiver<ImuList>(topic, "capnp:ImuList", std::move(receiver));
        }
        ReceiverId install(std::string_view topic, std::unique_ptr<Receiver<TagDetections>> receiver) override {
            return installEcalReceiver<TagDetections>(topic, "capnp:TagDetections", std::move(receiver));
        }
        ReceiverId install(std::string_view topic, std::unique_ptr<Receiver<Disparity>> receiver) override {
            return installEcalReceiver<Disparity>(topic, "capnp:Disparity", std::move(receiver));
        }
        ReceiverId install(std::string_view topic, std::unique_ptr<Receiver<Odometry3d>> receiver) override {
            return installEcalReceiver<Odometry3d>(topic, "capnp:Odometry3d", std::move(receiver));
        }
        ReceiverId install(std::string_view topic, std::unique_ptr<Receiver<HFOpticalFlowResult>> receiver) override {
            return installEcalReceiver<HFOpticalFlowResult>(topic, "capnp:HFOpticalFlowResult", std::move(receiver));
        }
        ReceiverId install(std::string_view topic, std::unique_ptr<Receiver<VioState>> receiver) override {
            return installEcalReceiver<VioState>(topic, "capnp:VioState", std::move(receiver));
        }
        ReceiverId install(std::string_view topic, std::unique_ptr<Receiver<Detections2d>> receiver) override {
            return installEcalReceiver<Detections2d>(topic, "capnp:Detections2d", std::move(receiver));
        }
        ReceiverId install(std::string_view topic, std::unique_ptr<Receiver<PointCloud>> receiver) override {
            return installEcalReceiver<PointCloud>(topic, "capnp:PointCloud", std::move(receiver));
        }
        ReceiverId install(std::string_view topic, std::unique_ptr<Receiver<CameraControl>> receiver) override {
            return installEcalReceiver<CameraControl>(topic, "capnp:CameraControl", std::move(receiver));
        }

        void remove(ReceiverId id) override {
            mEcalSubscribers.erase(id);
            std::lock_guard lock(mWebSocketReceivers->mMutex);
            mWebSocketReceivers->mReceivers.erase(id);
        }

        void clear() override {
            mEcalSubscribers.clear();
            std::lock_guard lock(mWebSocketReceivers->mMutex);
            mWebSocketReceivers->mReceivers.clear();
        }

    private:
        std::shared_ptr<FiltererState> mState;
        std::shared_ptr<VisualKitWebSocketReceivers> mWebSocketReceivers;
        std::map<ReceiverId, eCAL::CSubscriber> mEcalSubscribers;
        uint64_t mNextReceiverId;
    };


    template<typename T>
    class ECALReceiver: public Receiver<T> {
    public:
        ECALReceiver(eCAL::CPublisher publisher) : mPublisher(std::move(publisher)) {}
        ~ECALReceiver() {}

        ReceiverStatus handle(const Message<Shared<T>>& message) override {
            kj::VectorOutputStream buffer;
            message.payload.serialize(buffer);
            auto arrayPtr = buffer.getArray();
            mPublisher.Send(arrayPtr.begin(), arrayPtr.size());
            return ReceiverStatus::Open;
        }
        
    private:
        eCAL::CPublisher mPublisher;
    };

    class StringOutputStream : public kj::OutputStream {
    public:
        virtual void write(const void* buffer, size_t size) {
            mBuffer.append(static_cast<const char*>(buffer), size);
        }

        std::string mBuffer;
    };

    class WebSocketReceiver: public Receiver<ManagerCommand> {
    public:
        WebSocketReceiver(std::shared_ptr<VisualKitWebSocket> webSocket) : mWebSocket(std::move(webSocket)) {}

        ReceiverStatus handle(const Message<Shared<ManagerCommand>>& message) override {
            StringOutputStream stream;
            message.payload.serialize(stream);

            std::lock_guard lock(mWebSocket->mMutex);
            mWebSocket->mSocket.send(stream.mBuffer, true);
            return ReceiverStatus::Open;
        }

    private:
        std::shared_ptr<VisualKitWebSocket> mWebSocket;
    };

    class VisualKitSink: public DataSink {
    public:
        VisualKitSink(std::shared_ptr<VisualKitWebSocket> webSocket) : mWebSocket(std::move(webSocket)) {
            mState = std::make_shared<FiltererState>();
        }

        ~VisualKitSink() {
            stop(false);
            mState->mMode = FiltererState::Mode::Terminated;
        }

        void start() override {
            mState->mMode = FiltererState::Mode::Capturing;
        }

        void stop(bool waitForExhaustion) override {
            if (waitForExhaustion) {
                mState->waitTillNoReceivers();
            }

            mState->mMode = FiltererState::Mode::Filtering;
        }

        template<typename T>
        inline std::unique_ptr<Receiver<T>> createReceiver(std::string_view topic, std::string_view topicType) {
            auto publisher = eCAL::CPublisher(std::string(topic), std::string(topicType));

            if (!publisher.IsCreated()) {
                return nullptr;
            }

            auto receiver = std::make_unique<ECALReceiver<T>>(std::move(publisher));
            auto filteringReceiver = std::make_unique<FilteringReceiver<T>>(std::move(receiver), mState);
            return filteringReceiver;
        }

        std::unique_ptr<Receiver<ManagerMessage>> obtain(std::string_view topic, Type<ManagerMessage>) override {
            return nullptr;
        }
        std::unique_ptr<Receiver<ManagerCommand>> obtain(std::string_view topic, Type<ManagerCommand>) override {
            if (topic != "ws/command") {
                return nullptr;
            }

            auto receiver = std::make_unique<WebSocketReceiver>(mWebSocket);
            auto filteringReceiver = std::make_unique<FilteringReceiver<ManagerCommand>>(std::move(receiver), mState);
            return filteringReceiver;
        }
        std::unique_ptr<Receiver<Image>> obtain(std::string_view topic, Type<Image>) override {
            return createReceiver<Image>(topic, "capnp:Image");
        }
        std::unique_ptr<Receiver<Imu>> obtain(std::string_view topic, Type<Imu>) override {
            return createReceiver<Imu>(topic, "capnp:Imu");
        }
        std::unique_ptr<Receiver<GPS>> obtain(std::string_view topic, Type<GPS>) override {
            return createReceiver<GPS>(topic, "capnp:GPS");
        }
        std::unique_ptr<Receiver<ImuList>> obtain(std::string_view topic, Type<ImuList>) override {
            return createReceiver<ImuList>(topic, "capnp:ImuList");
        }
        std::unique_ptr<Receiver<TagDetections>> obtain(std::string_view topic, Type<TagDetections>) override {
            return createReceiver<TagDetections>(topic, "capnp:TagDetections");
        }
        std::unique_ptr<Receiver<Disparity>> obtain(std::string_view topic, Type<Disparity>) override {
            return createReceiver<Disparity>(topic, "capnp:Disparity");
        }
        std::unique_ptr<Receiver<Odometry3d>> obtain(std::string_view topic, Type<Odometry3d>) override {
            return createReceiver<Odometry3d>(topic, "capnp:Odometry3d");
        }
        std::unique_ptr<Receiver<HFOpticalFlowResult>> obtain(std::string_view topic, Type<HFOpticalFlowResult>) override {
            return createReceiver<HFOpticalFlowResult>(topic, "capnp:HFOpticalFlowResult");
        }
        std::unique_ptr<Receiver<VioState>> obtain(std::string_view topic, Type<VioState>) override {
            return createReceiver<VioState>(topic, "capnp:VioState");
        }
        std::unique_ptr<Receiver<Detections2d>> obtain(std::string_view topic, Type<Detections2d>) override {
            return createReceiver<Detections2d>(topic, "capnp:Detections2d");
        }
        std::unique_ptr<Receiver<PointCloud>> obtain(std::string_view topic, Type<PointCloud>) override {
            return createReceiver<PointCloud>(topic, "capnp:PointCloud");
        }
        std::unique_ptr<Receiver<CameraControl>> obtain(std::string_view topic, Type<CameraControl>) override {
            return createReceiver<CameraControl>(topic, "capnp:CameraControl");
        }

    private:
        std::shared_ptr<FiltererState> mState;
        std::shared_ptr<VisualKitWebSocket> mWebSocket;
    };


    struct VisualKitImpl : public VisualKit {
        DataSource& source() override {
            return *mSource;
        }

        DataSink& sink() override {
            return *mSink;
        }

        std::unique_ptr<VisualKitSource> mSource;
        std::unique_ptr<VisualKitSink> mSink;
    };

    std::unique_ptr<VisualKit> VisualKit::create(std::optional<std::string_view> manager) {
        if (!eCAL::IsInitialized()) {
            if (eCAL::Initialize() != 0) {
                return nullptr;
            }
        }
        
        ix::initNetSystem();

        auto webSocketReceivers = std::make_shared<VisualKitWebSocketReceivers>();
        auto websocket = std::make_shared<VisualKitWebSocket>();
        auto visualkit = std::make_unique<VisualKitImpl>();
        visualkit->mSource = std::make_unique<VisualKitSource>(webSocketReceivers);
        visualkit->mSink = std::make_unique<VisualKitSink>(websocket);

        if (manager.has_value()) {
            websocket->run(manager.value(), webSocketReceivers);
        }

        return visualkit;
    }
}