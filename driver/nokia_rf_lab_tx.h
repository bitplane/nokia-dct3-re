// license:BSD-3-Clause
// copyright-holders:Gaz
#ifndef MAME_NOKIA_NOKIA_RF_LAB_TX_H
#define MAME_NOKIA_NOKIA_RF_LAB_TX_H
#include <array>
#include <cstdint>

// Finite observed P21/P27 framing, not a physical FIFO-capacity claim.
// The attachment owns its clock; this state supplies no GSM result or reply.
struct nokia_rf_lab_tx
{
	std::array<std::uint16_t,15> words{};
	unsigned count=0;
	bool loading=false, ready=false, overflow=false;
	void reset() { *this={}; }
	bool control(std::uint16_t value)
	{
		value&=0x0e00;
		if (value==0x0a00) { reset(); loading=true; return false; }
		if (!loading) return false;
		loading=false;
		ready=value==0x0600 && count==words.size() && !overflow;
		return value==0x0600;
	}
	void write(std::uint16_t value)
	{
		if (!loading) return;
		if (count<words.size()) words[count++]=value;
		else overflow=true;
	}
	// A clock edge rejects unfinished work; a later commit cannot revive it.
	bool release(std::array<std::uint16_t,15> &output)
	{
		if (!ready) { reset(); return false; }
		output=words;
		ready=false;
		return true;
	}
};
#endif // MAME_NOKIA_NOKIA_RF_LAB_TX_H
