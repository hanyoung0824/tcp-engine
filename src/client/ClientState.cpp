#include "ClientState.hpp"

#include "ClientContext.hpp"
#include "EngineLog.hpp"

#include <boost/asio/steady_timer.hpp>

#include <algorithm>

namespace tcpengine
{
namespace
{
	class PassiveState final : public ClientState
	{
	public:
		PassiveState(ClientContext& context, ClientStateId id)
			: ClientState(context),
			m_id(id)
		{
		}

		ClientStateId getId() const override { return m_id; }

		const char* toString() const override
		{
			switch (m_id) {
			case ClientStateId::Idle: return "Idle";
			case ClientStateId::Starting: return "Starting";
			case ClientStateId::Stopped: return "Stopped";
			default: return "Passive";
			}
		}

		bool canChange(ClientStateId next) const override
		{
			switch (m_id) {
			case ClientStateId::Idle: return next == ClientStateId::Starting;
			// 시작 실패
			case ClientStateId::Starting: return next == ClientStateId::Connecting || next == ClientStateId::Stopped;
			default: return false;
			}
		}

		void onQuit() override {}

	private:
		const ClientStateId m_id;
	};

	class QuittingState final : public ClientState
	{
	public:
		using ClientState::ClientState;

		ClientStateId getId() const override { return ClientStateId::Quitting; }

		const char* toString() const override { return "Quitting"; }

		bool canChange(ClientStateId next) const override { return next == ClientStateId::Stopped; }

		void onEnter() override
		{
			m_context.sendLogout();
			m_context.closeConnection();
		}

		// 끝내는 중에 진행하던 재접속이 성공함. 남겨 두면 네트워크 스레드가 끝나지 않으므로 바로 닫음
		void onConnected(const std::shared_ptr<Connection>& connection) override
		{
			m_context.greet(connection);
			m_context.closeConnection();
		}

		void onDisconnected(const std::shared_ptr<Connection>& connection) override
		{
			m_context.notifyDisconnected(connection, m_context.takeDisconnectInfo(false));
		}

		void onQuit() override {}
	};

	class DisconnectedState final : public ClientState
	{
	public:
		using ClientState::ClientState;

		ClientStateId getId() const override { return ClientStateId::Disconnected; }

		const char* toString() const override { return "Disconnected"; }

		bool canChange(ClientStateId next) const override { return next == ClientStateId::Quitting; }
	};

	class ConnectedState final : public ClientState
	{
	public:
		using ClientState::ClientState;

		ClientStateId getId() const override { return ClientStateId::Connected; }

		const char* toString() const override { return "Connected"; }

		bool canChange(ClientStateId next) const override
		{
			return next == ClientStateId::WaitingReconnect || next == ClientStateId::Disconnected || next == ClientStateId::Quitting;
		}

		void onDisconnected(const std::shared_ptr<Connection>& connection) override;
	};

	// attempt번째 재접속을 1, 2, 4초…로 늘려 가며 maxDelay까지 기다렸다가 시도함
	// 이 상태를 떠나면 타이머가 함께 사라지며 취소됨
	class WaitingReconnectState final : public ClientState
	{
	public:
		WaitingReconnectState(ClientContext& context, int attempt)
			: ClientState(context),
			m_attempt(attempt),
			m_timer(context.getExecutor())
		{
		}

		ClientStateId getId() const override { return ClientStateId::WaitingReconnect; }

		const char* toString() const override { return "WaitingReconnect"; }

		bool canChange(ClientStateId next) const override
		{
			return next == ClientStateId::Connecting || next == ClientStateId::Quitting;
		}

		void onEnter() override
		{
			const std::chrono::seconds delay = delayFor(m_attempt, m_context.getReconnectMaxDelay());
			engineLogInfo("Reconnecting in ", delay.count(), "s (attempt ", m_attempt, ")");
			m_timer.expires_after(delay);
			// 시간이 된 뒤 콜백이 실행되기 전에 이 상태를 떠날 수 있어 this 대신 context로 지금 상태에 알림
			m_timer.async_wait([&context = m_context](const boost::system::error_code& ec) {
				if (!ec) {
					context.dispatch([](ClientState& state) { state.onReconnectDue(); });
				}
			});
		}

		void onReconnectDue() override;

	private:
		static std::chrono::seconds delayFor(int attempt, std::chrono::seconds maxDelay)
		{
			std::chrono::seconds delay(1);
			for (int i = 1; i < attempt && delay < maxDelay; ++i) {
				delay *= 2;
			}
			return std::min(delay, maxDelay);
		}

	private:
		const int m_attempt;
		boost::asio::steady_timer m_timer;
	};

	// attempt가 0이면 처음 접속. 처음 접속은 run()이 시작하고 실패도 run()이 처리함
	class ConnectingState final : public ClientState
	{
	public:
		ConnectingState(ClientContext& context, int attempt)
			: ClientState(context),
			m_attempt(attempt)
		{
		}

		ClientStateId getId() const override { return ClientStateId::Connecting; }

		const char* toString() const override { return "Connecting"; }

		// 처음 접속에 실패하면 Stopped
		bool canChange(ClientStateId next) const override
		{
			return next == ClientStateId::Connected || next == ClientStateId::WaitingReconnect
				|| next == ClientStateId::Quitting || next == ClientStateId::Stopped;
		}

		void onEnter() override
		{
			if (m_attempt > 0) {
				m_context.reconnect();
			}
		}

		void onConnected(const std::shared_ptr<Connection>& connection) override
		{
			m_context.changeState(std::make_unique<ConnectedState>(m_context));
			m_context.greet(connection);
		}

		void onConnectFailed() override
		{
			m_context.changeState(std::make_unique<WaitingReconnectState>(m_context, m_attempt + 1));
		}

	private:
		const int m_attempt;
	};

	void ConnectedState::onDisconnected(const std::shared_ptr<Connection>& connection)
	{
		const DisconnectInfo info = m_context.takeDisconnectInfo(true);
		if (info.willReconnect) {
			m_context.changeState(std::make_unique<WaitingReconnectState>(m_context, 1));
		}
		else {
			m_context.changeState(std::make_unique<DisconnectedState>(m_context));
		}
		m_context.notifyDisconnected(connection, info);
	}

	void WaitingReconnectState::onReconnectDue()
	{
		m_context.changeState(std::make_unique<ConnectingState>(m_context, m_attempt));
	}
}

void ClientState::onQuit()
{
	m_context.changeState(std::make_unique<QuittingState>(m_context));
}

std::unique_ptr<ClientState> makePassiveState(ClientContext& context, ClientStateId id)
{
	return std::make_unique<PassiveState>(context, id);
}

std::unique_ptr<ClientState> makeFirstConnectingState(ClientContext& context)
{
	return std::make_unique<ConnectingState>(context, 0);
}
}
