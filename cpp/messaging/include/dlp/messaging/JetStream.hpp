#ifndef DLP_MESSAGING_JET_STREAM_HPP
#define DLP_MESSAGING_JET_STREAM_HPP

#include <chrono>
#include <memory>
#include <optional>
#include <string>

#include "dlp/messaging/Outbox.hpp"

namespace dlp::messaging
{

class JetStreamMessage final
{
public:
    ~JetStreamMessage();

    JetStreamMessage(JetStreamMessage&& other) noexcept;
    JetStreamMessage& operator=(JetStreamMessage&& other) noexcept;

    JetStreamMessage(const JetStreamMessage&) = delete;
    JetStreamMessage& operator=(const JetStreamMessage&) = delete;

    [[nodiscard]] const EventEnvelope& Event() const noexcept;
    void Ack();

private:
    class Impl;
    explicit JetStreamMessage(std::unique_ptr<Impl> implementation);
    std::unique_ptr<Impl> implementation_;

    friend class JetStreamConsumer;
};

class JetStreamPublisher final : public EventPublisher
{
public:
    explicit JetStreamPublisher(std::string url);
    ~JetStreamPublisher() override;

    JetStreamPublisher(JetStreamPublisher&& other) noexcept;
    JetStreamPublisher& operator=(JetStreamPublisher&& other) noexcept;

    JetStreamPublisher(const JetStreamPublisher&) = delete;
    JetStreamPublisher& operator=(const JetStreamPublisher&) = delete;

    void EnsureEventStream();
    void Publish(const EventEnvelope& event) override;

private:
    class Impl;
    std::unique_ptr<Impl> implementation_;
};

class JetStreamConsumer final
{
public:
    JetStreamConsumer(
        std::string url,
        std::string subject,
        std::string durableName
    );
    ~JetStreamConsumer();

    JetStreamConsumer(JetStreamConsumer&& other) noexcept;
    JetStreamConsumer& operator=(JetStreamConsumer&& other) noexcept;

    JetStreamConsumer(const JetStreamConsumer&) = delete;
    JetStreamConsumer& operator=(const JetStreamConsumer&) = delete;

    [[nodiscard]] std::optional<JetStreamMessage> Fetch(std::chrono::milliseconds timeout);

private:
    class Impl;
    std::unique_ptr<Impl> implementation_;
};

}

#endif
