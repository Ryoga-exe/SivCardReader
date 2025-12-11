# pragma once
# include <Siv3D.hpp>

class CardReaderWin
{
public:
	using IDm = std::array<uint8, 8>;

	CardReaderWin() = default;

	~CardReaderWin();

	void startScan();

	void stopScan();

	bool isReady() const;

	bool isOK() const;

	IDm getIDm() const;

private:
	IDm m_idm = {};
	AsyncTask<void> m_task;
};
