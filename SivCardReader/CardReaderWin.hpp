# pragma once
# include <Siv3D.hpp>
# include <array>
# include <atomic>
# include <mutex>

class CardReaderWin
{
public:
	using IDm = std::array<uint8, 8>;

	CardReaderWin() = default;
	~CardReaderWin() { stopScan(); }

	CardReaderWin(const CardReaderWin&) = delete;
	CardReaderWin& operator=(const CardReaderWin&) = delete;

	void startScan();

	void stopScan();

	[[nodiscard]]
	bool isReady() const;

	[[nodiscard]]
	bool isOK() const;

	[[nodiscard]]
	IDm getIDm() const;

private:
	static void Read(CardReaderWin* self);

	IDm m_idm = {};

	AsyncTask<void> m_task;
	mutable std::mutex m_mutex;

	std::atomic<bool> m_running{ false };
	std::atomic<bool> m_ready{ false };
	std::atomic<bool> m_ok{ true };
};
