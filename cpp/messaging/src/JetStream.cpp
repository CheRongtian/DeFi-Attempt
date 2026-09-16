#include "dlp/messaging/JetStream.hpp"

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include <nats.h>

namespace dlp::messaging
{

namespace
{

struct ConnectionDeleter
{
    void operator()(natsConnection* connection) const noexcept
    {
        natsConnection_Destroy(connection);
    }
};

struct ContextDeleter
{
    void operator()(jsCtx* context) const noexcept
    {
        jsCtx_Destroy(context);
    }
};

struct SubscriptionDeleter
{
    void operator()(natsSubscription* subscription) const noexcept
    {
        natsSubscription_Destroy(subscription);
    }
};

struct MessageDeleter
{
    void operator()(natsMsg* message) const noexcept
    {
        natsMsg_Destroy(message);
    }
};

struct StreamInfoDeleter
{
    void operator()(jsStreamInfo* information) const noexcept
    {
        jsStreamInfo_Destroy(information);
    }
};

using Connection = std::unique_ptr<natsConnection, ConnectionDeleter>;
using Context = std::unique_ptr<jsCtx, ContextDeleter>;
using Subscription = std::unique_ptr<natsSubscription, SubscriptionDeleter>;
using Message = std::unique_ptr<natsMsg, MessageDeleter>;
using StreamInfo = std::unique_ptr<jsStreamInfo, StreamInfoDeleter>;

void Check(natsStatus status)
{
    if(status != NATS_OK)
    {
        throw std::runtime_error(natsStatus_GetText(status));
    }
}

struct JetStreamContext
{
    explicit JetStreamContext(const std::string& url)
    {
        natsConnection* rawConnection = nullptr;
        Check(natsConnection_ConnectTo(&rawConnection, url.c_str()));
        connection.reset(rawConnection);

        jsCtx* rawContext = nullptr;
        Check(natsConnection_JetStream(&rawContext, connection.get(), nullptr));
        context.reset(rawContext);
    }

    Connection connection;
    Context context;
};

void EnsureEventStream(jsCtx* context)
{
    jsStreamInfo* rawInformation = nullptr;
    const auto status = js_GetStreamInfo(
        &rawInformation,
        context,
        EVENT_STREAM.data(),
        nullptr,
        nullptr
    );
    StreamInfo information{rawInformation};
    if(status == NATS_OK)
    {
        return;
    }
    if(status != NATS_NOT_FOUND)
    {
        Check(status);
    }

    const char* subjects[]{"chain.>", "risk.>"};
    jsStreamConfig config{};
    Check(jsStreamConfig_Init(&config));
    config.Name = EVENT_STREAM.data();
    config.Subjects = subjects;
    config.SubjectsLen = 2;
    config.Storage = js_FileStorage;
    config.Replicas = 1;
    config.Duplicates = 120'000'000'000;

    rawInformation = nullptr;
    Check(js_AddStream(&rawInformation, context, &config, nullptr, nullptr));
    information.reset(rawInformation);
}

}

class JetStreamMessage::Impl final
{
public:
    Impl(Message message, EventEnvelope event)
        : message_(std::move(message)), event_(std::move(event))
    {
    }

    Message message_;
    EventEnvelope event_;
};

JetStreamMessage::JetStreamMessage(std::unique_ptr<Impl> implementation)
    : implementation_(std::move(implementation))
{
}

JetStreamMessage::~JetStreamMessage() = default;
JetStreamMessage::JetStreamMessage(JetStreamMessage&& other) noexcept = default;
JetStreamMessage& JetStreamMessage::operator=(JetStreamMessage&& other) noexcept = default;

const EventEnvelope& JetStreamMessage::Event() const noexcept
{
    return implementation_->event_;
}

void JetStreamMessage::Ack()
{
    Check(natsMsg_AckSync(implementation_->message_.get(), nullptr, nullptr));
}

class JetStreamPublisher::Impl final
{
public:
    explicit Impl(const std::string& url)
        : jetStream_(url)
    {
    }

    JetStreamContext jetStream_;
};

JetStreamPublisher::JetStreamPublisher(std::string url)
    : implementation_(std::make_unique<Impl>(url))
{
}

JetStreamPublisher::~JetStreamPublisher() = default;
JetStreamPublisher::JetStreamPublisher(JetStreamPublisher&& other) noexcept = default;
JetStreamPublisher& JetStreamPublisher::operator=(JetStreamPublisher&& other) noexcept = default;

void JetStreamPublisher::EnsureEventStream()
{
    dlp::messaging::EnsureEventStream(implementation_->jetStream_.context.get());
}

void JetStreamPublisher::Publish(const EventEnvelope& event)
{
    const auto payload = Serialize(event);
    jsPubOptions options{};
    Check(jsPubOptions_Init(&options));
    options.MsgId = event.eventId.c_str();

    Check(js_Publish(
        nullptr,
        implementation_->jetStream_.context.get(),
        event.eventType.c_str(),
        payload.data(),
        static_cast<int>(payload.size()),
        &options,
        nullptr
    ));
}

class JetStreamConsumer::Impl final
{
public:
    Impl(std::string url, std::string subject, std::string durableName)
        : jetStream_(url), subject_(std::move(subject)), durableName_(std::move(durableName))
    {
        dlp::messaging::EnsureEventStream(jetStream_.context.get());

        jsSubOptions options{};
        Check(jsSubOptions_Init(&options));
        options.Stream = EVENT_STREAM.data();
        options.ManualAck = true;
        options.Config.AckPolicy = js_AckExplicit;
        options.Config.DeliverPolicy = js_DeliverAll;

        natsSubscription* rawSubscription = nullptr;
        Check(js_PullSubscribe(
            &rawSubscription,
            jetStream_.context.get(),
            subject_.c_str(),
            durableName_.c_str(),
            nullptr,
            &options,
            nullptr
        ));
        subscription_.reset(rawSubscription);
    }

    JetStreamContext jetStream_;
    std::string subject_;
    std::string durableName_;
    Subscription subscription_;
};

JetStreamConsumer::JetStreamConsumer(
    std::string url,
    std::string subject,
    std::string durableName
)
    : implementation_(std::make_unique<Impl>(
        std::move(url),
        std::move(subject),
        std::move(durableName)
    ))
{
}

JetStreamConsumer::~JetStreamConsumer() = default;
JetStreamConsumer::JetStreamConsumer(JetStreamConsumer&& other) noexcept = default;
JetStreamConsumer& JetStreamConsumer::operator=(JetStreamConsumer&& other) noexcept = default;

std::optional<JetStreamMessage> JetStreamConsumer::Fetch(std::chrono::milliseconds timeout)
{
    natsMsgList list{};
    const auto status = natsSubscription_Fetch(
        &list,
        implementation_->subscription_.get(),
        1,
        timeout.count(),
        nullptr
    );
    if(status == NATS_TIMEOUT)
    {
        natsMsgList_Destroy(&list);
        return std::nullopt;
    }
    if(status != NATS_OK)
    {
        natsMsgList_Destroy(&list);
        Check(status);
    }

    Message message{list.Msgs[0]};
    list.Msgs[0] = nullptr;
    natsMsgList_Destroy(&list);

    const auto data = natsMsg_GetData(message.get());
    const auto length = natsMsg_GetDataLength(message.get());
    auto event = Deserialize(std::string_view{data, static_cast<std::size_t>(length)});
    return JetStreamMessage{
        std::make_unique<JetStreamMessage::Impl>(std::move(message), std::move(event))
    };
}

}
