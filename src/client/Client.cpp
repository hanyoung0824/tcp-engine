#include "tcpengine/client/Client.hpp"

#include "ClientContext.hpp"
#include "ClientState.hpp"
#include "ClientTransportFactory.hpp"
#include "ConnectionEvents.hpp"
#include "ControlPackets.hpp"
#include "EngineLog.hpp"
#include "HeartbeatChecker.hpp"
#include "Logger.hpp"
#include "PacketRouter.hpp"

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>

#include <exception>
#include <future>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>

namespace tcpengine
{
    struct Client::WorkGuard
    {
            boost::asio::executor_work_guard<boost::asio::io_context::executor_type> guard;
    };

    Client::Client(const ClientSettings &settings, std::uint16_t protocolVersion)
        : m_ioContext(std::make_unique<boost::asio::io_context>()), m_packetRouter(std::make_unique<PacketRouter>()),
          m_settings(settings), m_protocolVersion(protocolVersion), m_context(std::make_unique<ClientContext>(*this)),
          m_state(makePassiveState(*m_context, ClientStateId::Idle))
    {
    }

    Client::~Client()
    {
        stopNetworkThread();
    }

    bool Client::addPacketDispatcher(std::uint16_t id, detail::PacketDispatcher dispatcher)
    {
        if (m_state->getId() != ClientStateId::Idle)
        {
            engineLogError("registerHandler() must be called before run()");
            return false;
        }

        return m_packetRouter->addDispatcher(id, std::move(dispatcher));
    }

    void Client::send(const PacketBase &packet)
    {
        if (!m_transport)
        {
            return;
        }

        if (auto buffer = packet.serialize())
        {
            m_transport->send(std::move(buffer));
        }
    }

    bool Client::isConnected() const
    {
        return m_state->getId() == ClientStateId::Connected;
    }

    void Client::quit()
    {
        {
            const std::lock_guard lock(m_mainEventMutex);
            m_quitRequested = true;
        }
        m_mainEventReady.notify_one();
    }

    int Client::run()
    {
        Logger::installTerminateHandler();
        const ScopedLogging logging(m_settings.log);

        if (!logging.isStarted())
        {
            return 1;
        }

        if (m_state->getId() != ClientStateId::Idle)
        {
            engineLogError("run() can only be called once");
            return 1;
        }

        if (!validateSettings())
        {
            return 1;
        }

        changeState(makePassiveState(*m_context, ClientStateId::Starting));
        finishStateChange();

        if (!callOnStart() || !createComponents())
        {
            return stopWithError();
        }

        std::future<bool> firstConnect = postFirstConnect();
        startNetworkThread();

        if (!firstConnect.get())
        {
            return stopWithError();
        }

        runMainLoop();

        dispatch([](ClientState &state) { state.onQuit(); });
        stopNetworkThread();
        // 연결이 닫힌 일까지 응용에 알림
        processMainEvents();
        changeState(makePassiveState(*m_context, ClientStateId::Stopped));
        finishStateChange();
        return 0;
    }

    int Client::stopWithError()
    {
        stopNetworkThread();
        changeState(makePassiveState(*m_context, ClientStateId::Stopped));
        finishStateChange();
        return 1;
    }

    void Client::changeState(std::unique_ptr<ClientState> next)
    {
        if (!m_state->canChange(next->getId()))
        {
            engineLogError("Unexpected client state change: ", m_state->toString(), " -> ", next->toString());
        }

        engineLogDebug("Client state: ", m_state->toString(), " -> ", next->toString());
        m_retiredStates.push_back(std::move(m_state));
        m_state = std::move(next);
        m_enterPending = true;
    }

    void Client::dispatch(const std::function<void(ClientState &)> &event)
    {
        ++m_dispatchDepth;
        event(*m_state);
        --m_dispatchDepth;

        if (m_dispatchDepth == 0)
        {
            finishStateChange();
        }
    }

    void Client::finishStateChange()
    {
        // onEnter()에서 상태가 다시 바뀌면 이어서 처리
        while (m_enterPending)
        {
            m_enterPending = false;
            m_retiredStates.clear();
            m_state->onEnter();
        }

        m_retiredStates.clear();
    }

    bool Client::validateSettings() const
    {
        try
        {
            m_settings.validate();
            return true;
        }
        catch (const std::exception &e)
        {
            engineLogError("Invalid client settings: ", e.what());
            return false;
        }
    }

    bool Client::callOnStart()
    {
        try
        {
            return onStart();
        }
        catch (const std::exception &e)
        {
            engineLogError("onStart() failed: ", e.what());
            return false;
        }
    }

    bool Client::createComponents()
    {
        const boost::asio::any_io_executor executor = m_ioContext->get_executor();

        m_transport = makeClientTransport(executor, m_settings);

        if (!m_transport)
        {
            engineLogError("Unsupported transport");
            return false;
        }

        // 클라이언트는 통계를 보고하지 않으므로 쌓기만 함
        m_heartbeatChecker = std::make_unique<HeartbeatChecker>(executor, std::make_shared<NetworkStats>());

        m_packetRouter->setControlHandler(ControlType::ServerHello,
                                          [this](const std::shared_ptr<Connection> &connection, PacketReader &body) {
                                              return handleServerHello(connection, body);
                                          });
        m_packetRouter->setControlHandler(
            ControlType::Disconnect,
            [this](const std::shared_ptr<Connection> &, PacketReader &body) { return handleDisconnectNotice(body); });
        m_packetRouter->setControlHandler(
            ControlType::SessionStarted,
            [this](const std::shared_ptr<Connection> &, PacketReader &body) { return handleSessionStarted(body); });
        m_packetRouter->setControlHandler(
            ControlType::SessionResumed,
            [this](const std::shared_ptr<Connection> &, PacketReader &body) { return handleSessionResumed(body); });
        m_packetRouter->setControlHandler(
            ControlType::ResumeFailed,
            [this](const std::shared_ptr<Connection> &, PacketReader &body) { return handleResumeFailed(body); });

        m_connectionEvents = std::make_shared<const ConnectionEvents>(
            [this](const std::shared_ptr<Connection> &connection) { handleConnected(connection); },
            [this](const std::shared_ptr<Connection> &connection, std::span<const std::uint8_t> data) {
                m_heartbeatChecker->onReceived(*connection);
                return m_packetRouter->processReceived(connection, data);
            },
            [this](const std::shared_ptr<Connection> &connection) { handleDisconnected(connection); });
        return true;
    }

    std::future<bool> Client::postFirstConnect()
    {
        changeState(makeFirstConnectingState(*m_context));
        finishStateChange();
        auto result = std::make_shared<std::promise<bool>>();
        std::future<bool> connected = result->get_future();
        connect([result](bool success) { result->set_value(success); });
        return connected;
    }

    void Client::startNetworkThread()
    {
        m_workGuard = std::make_unique<WorkGuard>(WorkGuard{boost::asio::make_work_guard(*m_ioContext)});
        m_networkThread = std::thread([this] { m_ioContext->run(); });
    }

    void Client::stopNetworkThread()
    {
        if (!m_networkThread.joinable())
        {
            return;
        }

        m_transport->close();
        m_workGuard.reset();
        m_networkThread.join();
    }

    void Client::runMainLoop()
    {
        std::chrono::steady_clock::time_point nextTick = std::chrono::steady_clock::now();

        while (!m_quitRequested)
        {
            processMainEvents();

            const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

            if (now >= nextTick)
            {
                dispatch([now](ClientState &state) { state.onTick(now); });
                onUpdate();
                nextTick += m_settings.tickInterval;

                // 응용이 오래 걸려 밀렸으면 쌓인 틱을 몰아서 돌리지 않음
                if (nextTick < now)
                {
                    nextTick = now + m_settings.tickInterval;
                }
            }

            std::unique_lock lock(m_mainEventMutex);
            m_mainEventReady.wait_until(lock, nextTick, [this] { return !m_mainEvents.empty() || m_quitRequested; });
        }
    }

    void Client::processMainEvents()
    {
        std::deque<std::function<void()>> events;
        {
            const std::lock_guard lock(m_mainEventMutex);
            events.swap(m_mainEvents);
        }

        for (const std::function<void()> &event : events)
        {
            event();
        }
    }

    void Client::postToMain(std::function<void()> event)
    {
        {
            const std::lock_guard lock(m_mainEventMutex);
            m_mainEvents.push_back(std::move(event));
        }
        m_mainEventReady.notify_one();
    }

    void Client::runPacketHandler(std::uint16_t packetId, const std::function<void()> &handle)
    {
        try
        {
            handle();
        }
        catch (const std::exception &e)
        {
            engineLogError("Exception while handling packet id ", packetId, ": ", e.what());
            m_transport->close();
        }
        catch (...)
        {
            engineLogError("Unknown exception while handling packet id ", packetId);
            m_transport->close();
        }
    }

    void Client::connect(std::function<void(bool connected)> onResult)
    {
        m_transport->connect(m_settings.host, m_settings.port, m_settings.connectTimeout, m_connectionEvents,
                             std::move(onResult));
    }

    void Client::reconnect()
    {
        connect([this](bool connected) {
            if (!connected)
            {
                postToMain([this] { dispatch([](ClientState &state) { state.onConnectFailed(); }); });
            }
        });
    }

    void Client::handleConnected(const std::shared_ptr<Connection> &)
    {
        postToMain([this] { dispatch([](ClientState &state) { state.onConnected(); }); });
    }

    void Client::handleDisconnected(const std::shared_ptr<Connection> &connection)
    {
        m_heartbeatChecker->stop(*connection);
        postToMain([this] { dispatch([](ClientState &state) { state.onDisconnected(); }); });
    }

    bool Client::handleServerHello(const std::shared_ptr<Connection> &connection, PacketReader &body)
    {
        const std::optional<control::ServerHello> hello = control::readServerHello(body);

        if (!hello)
        {
            return false;
        }

        m_heartbeatChecker->start(connection, hello->heartbeat, control::ping());
        return true;
    }

    bool Client::handleDisconnectNotice(PacketReader &body)
    {
        const std::optional<DisconnectReason> reason = control::readDisconnect(body);

        if (!reason)
        {
            return false;
        }

        engineLogInfo("Server is closing the connection: ", toString(*reason));
        // 연결이 끊긴 일보다 먼저 메인 스레드에 도착함
        postToMain([this, reason] { m_disconnectReason = reason; });
        return true;
    }

    bool Client::handleSessionStarted(PacketReader &body)
    {
        std::string token;

        if (!body.readString(token) || !body.isEnd() || token.size() != sessionTokenLength)
        {
            return false;
        }

        engineLogDebug("Session started");
        postToMain([this, token = std::move(token)]() mutable { m_sessionToken = std::move(token); });
        return true;
    }

    bool Client::handleSessionResumed(PacketReader &body)
    {
        if (!body.isEnd())
        {
            return false;
        }

        engineLogInfo("Session resumed");
        postToMain([this] { onSessionResumed(); });
        return true;
    }

    bool Client::handleResumeFailed(PacketReader &body)
    {
        if (!body.isEnd())
        {
            return false;
        }

        engineLogInfo("Session resume failed, login again");
        postToMain([this] {
            m_sessionToken.clear();
            onSessionLost();
        });
        return true;
    }

    void ClientContext::changeState(std::unique_ptr<ClientState> next)
    {
        m_client.changeState(std::move(next));
    }

    std::chrono::seconds ClientContext::getReconnectMaxDelay() const
    {
        return m_client.m_settings.reconnectMaxDelay;
    }

    void ClientContext::greet()
    {
        m_client.m_transport->send(
            control::clientHello(control::ClientHello{engineProtocolVersion, m_client.m_protocolVersion}));

        if (!m_client.m_sessionToken.empty())
        {
            m_client.m_transport->send(control::make(ControlType::Resume, m_client.m_sessionToken));
        }

        m_client.m_connectionAnnounced = true;
        m_client.onConnected();
    }

    void ClientContext::closeConnection()
    {
        m_client.m_transport->close();
    }

    void ClientContext::reconnect()
    {
        m_client.reconnect();
    }

    void ClientContext::sendLogout()
    {
        if (!m_client.m_sessionToken.empty())
        {
            m_client.m_transport->send(control::make(ControlType::Logout));
        }
    }

    DisconnectInfo ClientContext::takeDisconnectInfo(bool mayReconnect)
    {
        DisconnectInfo info;
        info.reason = std::exchange(m_client.m_disconnectReason, std::nullopt);
        const bool reconnectable = !info.reason || canReconnectAfter(*info.reason);

        if (!reconnectable)
        {
            m_client.m_sessionToken.clear();
        }

        info.willReconnect = reconnectable && mayReconnect && m_client.m_settings.reconnectMaxDelay.count() > 0;
        return info;
    }

    void ClientContext::notifyDisconnected(const DisconnectInfo &info)
    {
        if (!std::exchange(m_client.m_connectionAnnounced, false))
        {
            return;
        }

        m_client.onDisconnected(info);
    }
} // namespace tcpengine
