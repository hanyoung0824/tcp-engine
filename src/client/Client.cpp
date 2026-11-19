#include "tcpengine/client/Client.hpp"

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
          m_settings(settings), m_protocolVersion(protocolVersion), m_state(makeIdleState(*this))
    {
    }

    Client::~Client()
    {
        stopNetworkThread();
    }

    bool Client::addPacketDispatcher(std::uint16_t id, detail::PacketDispatcher dispatcher)
    {
        return m_state->tryRegisterHandler(id, std::move(dispatcher));
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
        return m_state->isConnected();
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

        if (!m_state->canRun())
        {
            engineLogError("run() can only be called once");
            return 1;
        }

        if (!validateSettings())
        {
            return 1;
        }

        changeState(makePassiveState(*this, ClientStateId::Starting));
        applyNextState();

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
        changeState(makePassiveState(*this, ClientStateId::Stopped));
        applyNextState();
        return 0;
    }

    int Client::stopWithError()
    {
        stopNetworkThread();
        changeState(makePassiveState(*this, ClientStateId::Stopped));
        applyNextState();
        return 1;
    }

    void Client::changeState(std::unique_ptr<ClientState> next)
    {
        m_nextState = std::move(next);
    }

    void Client::dispatch(const std::function<void(ClientState &)> &event)
    {
        event(*m_state);
        applyNextState();
    }

    void Client::applyNextState()
    {
        while (m_nextState)
        {
            if (!m_state->canChange(m_nextState->getId()))
            {
                engineLogError("Unexpected client state change: ", m_state->toString(), " -> ",
                               m_nextState->toString());
            }

            engineLogDebug("Client state: ", m_state->toString(), " -> ", m_nextState->toString());
            m_state = std::move(m_nextState);
            m_state->onEnter();
        }
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
        changeState(makeFirstConnectingState(*this));
        applyNextState();
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
} // namespace tcpengine
