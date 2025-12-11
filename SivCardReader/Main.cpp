# include <Siv3D.hpp> // Siv3D v0.6.16
# include "CardReaderWin.hpp"

String IDmToString(const CardReaderWin::IDm& idm)
{
	return U"{:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X}"_fmt(idm[0], idm[1], idm[2], idm[3], idm[4], idm[5], idm[6], idm[7]);
}

void Main()
{
	CardReaderWin reader;
	reader.startScan();

	Optional<String> idm;

	while (System::Update())
	{
		ClearPrint();

		if (reader.isReady() and not idm)
		{
			idm = IDmToString(reader.getIDm());
		}

		Print << U"Status: " << (reader.isOK() ? U"OK" : U"ERROR");
		Print << U"IDm: " << idm.value_or(U"waiting...");
	}
}
