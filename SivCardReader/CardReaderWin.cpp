# include "CardReaderWin.hpp"

# define NOMINMAX
# include <Windows.h>
# include <winscard.h>
# include <vector>
# include <span>
# pragma comment(lib, "winscard.lib")

void CardReaderWin::startScan()
{
	stopScan();

	m_ready = false;
	m_ok = true;
	m_running = true;

	m_task = AsyncTask<void>(Read, this);
}

void CardReaderWin::stopScan()
{
	if (m_running.exchange(false))
	{
		if (m_task.isValid())
		{
			m_task.wait();
		}
	}
}

bool CardReaderWin::isReady() const
{
	return m_ready.load();
}

bool CardReaderWin::isOK() const
{
	return m_ok.load();
}

CardReaderWin::IDm CardReaderWin::getIDm() const
{
	std::scoped_lock lock{ m_mutex };
	return m_idm;
}

// helpers
namespace {
	struct PcscContext
	{
		SCARDCONTEXT ctx{};

		~PcscContext()
		{
			if (ctx)
			{
				SCardReleaseContext(ctx);
			}
		}

		bool establish()
		{
			return (SCardEstablishContext(SCARD_SCOPE_USER, nullptr, nullptr, &ctx) == SCARD_S_SUCCESS);
		}
	};

	struct PcscCard
	{
		SCARDHANDLE handle{};

		~PcscCard()
		{
			if (handle)
			{
				SCardDisconnect(handle, SCARD_LEAVE_CARD);
			}
		}
	};

	Array<String> SplitMultiString(const wchar_t* msz)
	{
		Array<String> out;
		if (not msz)
		{
			return out;
		}
		const wchar_t* p = msz;
		while (*p)
		{
			out << Unicode::FromWstring(p); // UTF-16 to UTF-32
			p += (wcslen(p) + 1);
		}
		return out;
	}

	// PaSoRi / RC-S3xx を優先選択（見つからなければ先頭）
	size_t ChooseReaderIndex(const Array<String>& readers)
	{
		for (size_t i = 0; i < readers.size(); ++i)
		{
			const auto& r = readers[i];
			if (r.includes(U"Sony") || r.includes(U"PaSoRi") || r.includes(U"RC-S3"))
			{
				return i;
			}
		}
		return 0;
	}

	bool IsOKSW(const uint8* buf, DWORD len)
	{
		return (len >= 2 and buf[len - 2] == 0x90 and buf[len - 1] == 0x00);
	}
}


void CardReaderWin::Read(CardReaderWin* self)
{
	// PC/SC 初期化
	PcscContext c;
	if (not c.establish())
	{
		self->m_ok = false;
		self->m_running = false;
		return;
	}

	// リーダ列挙
	DWORD mszLen = 0;
	if (SCardListReadersW(c.ctx, nullptr, nullptr, &mszLen) != SCARD_S_SUCCESS or mszLen <= 2)
	{
		self->m_ok = false;
		self->m_running = false;
	}
	std::vector<wchar_t> msz(mszLen);
	if (SCardListReadersW(c.ctx, nullptr, msz.data(), &mszLen) != SCARD_S_SUCCESS) {
		self->m_ok = false;
		self->m_running = false;
		return;
	}

	const auto readers = SplitMultiString(msz.data());
	if (readers.isEmpty())
	{
		self->m_ok = false;
		self->m_running = false;
		return;
	}
	const std::wstring readerW = readers[ChooseReaderIndex(readers)].toWstr();

	// IDm 取れたら終了
	SCARD_READERSTATEW st{};
	st.szReader = readerW.c_str();
	st.dwCurrentState = SCARD_STATE_UNAWARE;

	while (self->m_running)
	{
		// 200ms タイムアウトで監視
		if (SCardGetStatusChangeW(c.ctx, 200, &st, 1) != SCARD_S_SUCCESS)
		{
			continue;
		}
		const bool present = (st.dwEventState & SCARD_STATE_PRESENT) != 0;
		st.dwCurrentState = st.dwEventState;
		if (!present)
		{
			continue;
		}

		// 接続
		PcscCard card;
		DWORD proto{};
		if (SCardConnectW(c.ctx, readerW.c_str(), SCARD_SHARE_SHARED, SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1, &card.handle, &proto) != SCARD_S_SUCCESS)
		{
			continue;
		}
		const SCARD_IO_REQUEST* const pci = (proto == SCARD_PROTOCOL_T0) ? SCARD_PCI_T0 : SCARD_PCI_T1;

		// IDm 取得
		std::array<uint8, 258> rIDm{};
		DWORD nIDm = static_cast<DWORD>(rIDm.size());
		static constexpr std::array<uint8, 5> CMD_IDM{ 0xFF, 0xCA, 0x00, 0x00, 0x00 };

		const bool ok =
			(SCardTransmit(card.handle, pci, CMD_IDM.data(), CMD_IDM.size(), nullptr, rIDm.data(), &nIDm) == SCARD_S_SUCCESS)
			and IsOKSW(rIDm.data(), nIDm)
			and nIDm >= 10;

		if (ok) {
			IDm got{};
			for (size_t i = 0; i < 8; ++i)
			{
				got[i] = rIDm[i];
			}

			{
				std::scoped_lock lock{ self->m_mutex };
				self->m_idm = got;
			}
			self->m_ready = true;
			self->m_ok = true;
			break;
		}
	}
	self->m_running = false;
}
