// license:BSD-3-Clause
// copyright-holders:Gaz

#include "emu.h"
#include "tms320c54x_test_state.h"

namespace c54x_test {

void tms320c54x_test_state::rom4_crc_input(unsigned vector, u16 (&words)[12])
{
	std::fill(std::begin(words), std::end(words), 0);
	if (vector < 192)
		words[vector / 16] = u16(1U << (vector % 16));
	else if (vector == 193)
		std::fill(std::begin(words), std::end(words), 0xffff);
	else if (vector > 193)
	{
		u32 value = 0x9e3779b9U ^ vector;
		for (u16 &word : words)
		{
			value ^= value << 13;
			value ^= value >> 17;
			value ^= value << 5;
			word = u16(value);
		}
	}
}

u16 tms320c54x_test_state::rom4_crc_expected(u16 const (&words)[12])
{
	// GSM 05.03 3.1.1: divide message*D^3 by D^3+D+1, then
	// complement parity. Keep this independent of C54x rotate instructions.
	u64 polynomial = 0;
	const auto append = [&polynomial](unsigned bit) { polynomial = (polynomial << 1) | bit; };
	u16 const combined = u16((words[10] >> 9) | (words[11] << 7));
	for (unsigned bit = 0; bit < 16; ++bit)
	{
		append(BIT(words[0], bit));
		append(BIT(combined, 15 - bit));
	}
	for (unsigned bit = 0; bit < 9; ++bit)
	{
		append(BIT(words[1], bit));
		append(BIT(words[10], 8 - bit));
	}
	polynomial <<= 3;
	for (int degree = 52; degree >= 3; --degree)
		if (BIT(polynomial, degree)) polynomial ^= u64(0xb) << (degree - 3);
	unsigned const parity = unsigned(polynomial) ^ 7;
	unsigned const reversed = (BIT(parity, 0) << 2) | (BIT(parity, 1) << 1) | BIT(parity, 2);
	return u16(reversed << 11);
}

void tms320c54x_test_state::start_rom4_crc_packed(unsigned index)
{
	u16 words[12];
	rom4_crc_input(index / 2, words);
	for (unsigned i = 0; i < 12; ++i) m_cpu->space(AS_DATA).write_word(0x1400 + i, words[i]);
	memregion("dspprg")->as_u16(0x0f00) = 0xf4e1;
	m_cpu->space(AS_DATA).write_word(0x1ec3, 0x0f00);
	m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x812f);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1ec3);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x1400);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x12345678);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xdeadbeef);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, index & 1 ? 0x0800 : 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x4101);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0xffac);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_phase = 64000 + index;
	m_check_timer->adjust(attotime::from_usec(100));
}

gsm::tch_f::speech_bits tms320c54x_test_state::rom4_fr_parameters(unsigned vector, u16 (&words)[76])
{
	gsm::tch_f::speech_bits serial{};
	u32 random = 0x9e3779b9U ^ vector;
	unsigned position = 0;
	for (unsigned p = 0; p < 76; ++p)
	{
		words[p] = 0;
		for (unsigned bit = 0; bit < ROM4_FR_WIDTHS[p]; ++bit, ++position)
		{
			random ^= random << 13;
			random ^= random >> 17;
			random ^= random << 5;
			serial[position] = vector < 260 ? position == vector : vector == 260 ? 0 : random & 1;
			words[p] = (words[p] << 1) | serial[position];
		}
	}
	return serial;
}

u8 tms320c54x_test_state::rom4_efr_crc_reference(std::array<u8, 244> const &serial)
{
	static constexpr unsigned protected_bits[] = {
		39,40,41,42,43,44,48,87,45,2,3,8,10,18,19,24,46,47,142,143,
		144,145,146,147,92,93,195,196,98,137,148,94,197,149,150,95,198,
		4,5,11,12,16,9,6,7,13,17,20,96,199,1,14,15,21,25,26,28,
		151,201,190,240,88,138,191,241};
	// TS 45.003 3.1.1.1: systematic division by x^8+x^4+x^3+x^2+1.
	unsigned remainder = 0;
	for (unsigned i = 0; i < 73; ++i)
	{
		remainder = (remainder << 1) | (i < 65 ? serial[protected_bits[i] - 1] : 0);
		if (remainder & 0x100) remainder ^= 0x11d;
	}
	u8 result = 0;
	for (unsigned bit = 0; bit < 8; ++bit) result = (result << 1) | BIT(remainder, bit);
	return result;
}

u8 tms320c54x_test_state::rom4_efr_parameters(unsigned vector, u16 (&parameters)[57], std::array<u8, 244> *speech)
{
	std::array<u8, 244> serial{};
	u32 random = 0x9e3779b9U ^ vector;
	unsigned position = 0;
	for (unsigned p = 0; p < 57; ++p)
	{
		parameters[p] = 0;
		for (unsigned bit = 0; bit < ROM4_EFR_WIDTHS[p]; ++bit, ++position)
		{
			random ^= random << 13; random ^= random >> 17; random ^= random << 5;
			serial[position] = vector < 244 ? position == vector : vector == 244 ? 0 : random & 1;
			parameters[p] = (parameters[p] << 1) | serial[position];
		}
	}
	if (speech) *speech = serial;
	return rom4_efr_crc_reference(serial);
}

std::array<u16, 17> tms320c54x_test_state::rom4_efr_packed(std::array<u8, 244> const &speech)
{
	// TS 45.003 table 6, independently transcribed one-based w indices.
	static constexpr unsigned importance[] = {
		39,40,41,42,43,44,146,147,148,149,150,151,94,95,201,202,
		48,89,100,141,45,152,96,203,2,3,8,10,18,19,24,46,47,153,154,
		97,204,4,5,11,12,16,9,6,7,13,17,20,98,205,
		1,14,15,21,25,26,28,155,207,196,248,90,142,197,249,
		253,254,255,256,257,258,259,260,49,101,156,208,22,23,27,29,
		52,56,60,64,68,104,108,112,116,120,159,163,167,171,175,
		211,215,219,223,227,91,143,198,250,50,102,157,209,
		30,31,32,33,34,35,36,99,206,53,57,
		61,65,69,105,109,113,117,121,160,164,168,172,176,
		212,216,220,224,228,54,58,62,66,106,110,114,118,
		161,165,169,173,213,221,225,92,144,199,251,51,103,158,210,
		93,145,200,252,55,59,63,67,107,111,115,119,162,166,170,
		174,214,222,226,37,38,
		70,72,73,122,124,125,177,179,180,229,231,232,217,218,
		71,123,178,230,74,77,80,83,86,126,129,132,135,138,
		181,184,187,190,193,233,236,239,242,245,
		75,78,81,84,87,127,130,133,136,139,182,185,188,191,194,
		234,237,240,243,246,
		76,79,82,85,88,128,131,134,137,140,183,186,189,192,195,
		235,238,241,244,247};
	static_assert(std::size(importance) == 260);
	u8 const crc = rom4_efr_crc_reference(speech);
	std::array<u8, 260> preliminary{};
	unsigned next = 0;
	for (unsigned i = 0; i < speech.size(); ++i)
	{
		preliminary[next++] = speech[i];
		if (i == 70 || i == 120 || i == 173 || i == 223)
			for (unsigned copy = 0; copy < 2; ++copy) preliminary[next++] = speech[i - 1];
	}
	for (unsigned bit = 0; bit < 8; ++bit) preliminary[next++] = BIT(crc, bit);
	std::array<u16, 17> packed{};
	unsigned position = 0;
	auto append = [&](int d) {
		packed[position / 16] = (packed[position / 16] << 1) | (d < 0 ? 0 : preliminary[importance[d] - 1]);
		++position;
	};
	// Native word layout, separately checked against all 272 ROM descriptors.
	for (int word = 0; word < 5; ++word)
		for (int d = word * 32 + 30; d >= word * 32; d -= 2) append(d);
	append(179); append(181); append(-1); append(-1); append(-1);
	for (int d = 180; d >= 160; d -= 2) append(d);
	for (int start : {147, 115, 83, 51, 19})
		for (int d = start; d < start + 32; d += 2) append(d);
	for (unsigned i = 0; i < 7; ++i) append(-1);
	for (int d = 1; d < 18; d += 2) append(d);
	for (int start : {182, 198, 214, 230})
		for (int d = start + 15; d >= start; --d) append(d);
	append(-1); append(-1);
	for (int d = 259; d >= 246; --d) append(d);
	return packed;
}

std::array<u16, 17> tms320c54x_test_state::rom4_efr_packed(unsigned vector)
{
	u16 parameters[57];
	std::array<u8, 244> speech;
	rom4_efr_parameters(vector, parameters, &speech);
	return rom4_efr_packed(speech);
}

void tms320c54x_test_state::start_rom4_delayed_repeat(unsigned index)
{
	auto &data = m_cpu->space(AS_DATA);
	unsigned const count = std::array<unsigned, 5>{0, 1, 2, 9, 39}[index / 2];
	bool const returning = index & 1;
	u16 *const program = &memregion("dspprg")->as_u16();
	program[0x0e00] = returning ? 0xfe00 : 0xf273;
	if (!returning) program[0x0e01] = 0x0e20;
	program[returning ? 0x0e01 : 0x0e02] = 0xec00 | count;
	program[returning ? 0x0e02 : 0x0e03] = 0xe5ab;
	program[returning ? 0x0e03 : 0x0e04] = 0xf4e1;
	program[0x0e20] = 0xf4e1;
	for (unsigned i = 0; i < 40; ++i)
	{
		data.write_word(0x1400 + i, 0x1357 + i * 257);
		data.write_word(0x1600 + i, 0xdead);
	}
	data.write_word(0x15ff, 0x1234); data.write_word(0x1628, 0x5678);
	data.write_word(0x1ec3, 0x0e20);
	m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0e00);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1ec3);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x1400);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR5, 0x1600);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x4101);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0xffac);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_rom4_codec_replay = false; m_rom4_codec_replays = 0;
	m_phase = 303000 + index;
	m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock()));
}

bool tms320c54x_test_state::rom4_codec_is(char const *name) const
{
	std::string_view bios(machine().options().bios());
	if(bios.size()>5 && bios.substr(bios.size()-5)=="-pipe") bios.remove_suffix(5);
	if(bios=="efr-dtx-enc") bios="efr-dtx-encoder";
	else if(bios=="efr-dtx-dec") bios="efr-dtx-decoder";
	else if(bios=="efr-ehome") bios="efr-enc-homing";
	else if(bios=="efr-isid") bios="efr-invalid-sid";
	else if(bios=="efr-lsid") bios="efr-lost-sid";
	else if(bios=="efr-lso") bios="efr-lsid-observe";
	else if(bios=="efr-loss") bios="efr-loss-observe";
	else if(bios=="efr-herr") bios="efr-home-errors";
	return bios==name;
}

void tms320c54x_test_state::enable_rom4_codec_pipeline(char const *kind)
{
	std::string_view const bios(machine().options().bios());
	if(bios.size()>5 && bios.substr(bios.size()-5)=="-pipe")
	{
		m_cpu->set_pipeline_frontend(true);
		osd_printf_info("ROM4 %s frontend: staged handset_audio_claim=0\n",kind);
	}
}

void tms320c54x_test_state::start_rom4_efr_encoder(unsigned frame, bool initialize)
{
	auto &data = m_cpu->space(AS_DATA);
	bool const dtx = rom4_codec_is("efr-dtx-encoder");
	bool const homing = rom4_codec_is("efr-enc-homing");
	u16 *const program = &memregion("dspprg")->as_u16();
	if (initialize)
	{
		expect(program[0xcc0e] == 0x7711 && program[0xcc0f] == 0x0ba8,
			"EFR encoder acquired reset entry is loaded before CPU execution");
		expect(data.read_word(0xea70) == 0x7530 && data.read_word(0xea71) == 0x6590,
			"EFR encoder acquired reset data table is loaded before CPU execution");
		data.write_word(0x0bac, dtx ? 4 : 0); // Encoder DTX mode is an explicit algorithm input.
		m_rom4_pcm_mismatches = 0;
		unsigned pc = 0x0e00;
		auto call = [&](u16 target) { program[pc++] = 0xf074; program[pc++] = target; };
		// Private caller reproduces the acquired 9f6a/9f7c/9f8e/9f90..9fb9/9e84 order.
		for (u16 target : {0xcf9a, 0xbed6, 0xbefa, 0xbf20, 0xbf92}) call(target);
		for (unsigned subframe = 0; subframe < 4; ++subframe)
		{
			program[pc++] = 0xe800 | subframe;
			program[pc++] = 0x80f8; program[pc++] = 0x0e2c;
			call(0xc093);
		}
		call(0xc273);
		program[pc] = 0xfc00;
	}
	else
	{
		u8 const *const input = memregion(dtx ? "efr_dtx_encoder_pcm" : homing ? "efr_homing_encoder_pcm" : "efr_encoder_pcm")->base() + frame * 320;
		for (unsigned i = 0; i < 160; ++i)
			data.write_word(0x0cf4 + i, input[i * 2] | (input[i * 2 + 1] << 8));
	}
	program[0x0f00] = 0xf4e1;
	data.write_word(0x1ec3, 0x0f00);
	m_cpu->set_state_int(tms320c54x_device::STATE_PC, initialize ? 0xcc0e : 0x0e00);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1ec3);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x4101);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0xffac);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_phase = initialize ? 304000 : 304001 + frame;
	if (initialize || !frame) m_rom4_fr_trace_count = 0;
	m_check_timer->adjust(attotime::from_msec(20));
	// Compare drained reset RAM before frame execution, not phase-skewed registers.
	if (!initialize && !frame && machine().options().verbose()) trace_rom4_fr_input(304001, 0x0e00);
	if (!initialize) save_rom4_codec_frame(frame);
}

tms320c54x_test_state::efr_decoder_fixture tms320c54x_test_state::rom4_efr_decoder_fixture() const
{
	if (rom4_codec_is("efr-dtx-decoder")) return { "efr_dtx_input", "efr_dtx_pcm", " DTX", 518 };
	if (rom4_codec_is("efr-invalid-sid")) return { "efr_invalid_sid_input", "efr_invalid_sid_pcm", " invalid SID", 518 };
	if (rom4_codec_is("efr-lost-sid") || rom4_codec_is("efr-lsid-observe")) return { "efr_lost_sid_input", "efr_lost_sid_pcm", " lost SID", 518 };
	if (rom4_codec_is("efr-erasure") || rom4_codec_is("efr-loss-observe")) return { "efr_erasure_input", "efr_erasure_pcm", " erasure", 163 };
	if (rom4_codec_is("efr-homing")) return { "efr_homing_input", "efr_homing_pcm", " homing", 46 };
	if (rom4_codec_is("efr-home-errors")) return { "efr_homing_error_input", "efr_homing_error_pcm", " homing error", 49 };
	return { "efr_reference_input", "efr_reference_pcm", "", 163 };
}

void tms320c54x_test_state::start_rom4_efr_decoder(unsigned frame, bool initialize)
{
	m_rom4_efr_pitch_observations = m_rom4_efr_code_observations = 0;
	m_rom4_homing_resets = 0;
	auto &data = m_cpu->space(AS_DATA);
	auto const fixture = rom4_efr_decoder_fixture();
	if (initialize)
	{
		data.write_word(0x0ac8, 0);
		m_rom4_pcm_mismatches = 0;
		m_rom4_cni_nonzero_frames = 0;
	}
	else
	{
		u8 const *const input = memregion(fixture.input)->base() + frame * 494;
		auto word = [&](unsigned index) { return u16(input[index * 2] | (input[index * 2 + 1] << 8)); };
		data.write_word(0x1343, word(0));
		unsigned bit = 1;
		for (unsigned p = 0; p < 57; ++p)
		{
			u16 value = 0;
			for (unsigned i = 0; i < ROM4_EFR_WIDTHS[p]; ++i) value = (value << 1) | word(bit++);
			data.write_word(0x1344 + p, value);
		}
		data.write_word(0x79, word(245));
		data.write_word(0x0ac8, (data.read_word(0x0ac8) & ~0x000c) | (word(0) << 2) | (word(246) << 3));
		data.write_word(0x130a, 0x1388);
		for (unsigned i = 0; i < 160; ++i) data.write_word(0x1388 + i, 0xdead);
		// The real caller unpacks through c8c7: c8d7..c8d9 leave AR2 at 1344.
		// The wrapper's first-subframe homing test consumes that entry pointer.
		auto &program = m_cpu->space(AS_PROGRAM);
		expect(program.read_word(0xc8d7) == 0x7712 && program.read_word(0xc8d8) == 0x1343 && program.read_word(0xc8d9) == 0x8092,
			"original EFR unpack caller leaves the homing parameter pointer at 1344");
	}
	memregion("dspprg")->as_u16(0x0f00) = 0xf4e1;
	data.write_word(0x1ec3, 0x0f00);
	m_cpu->set_state_int(tms320c54x_device::STATE_PC, initialize ? 0xc941 : 0xc8dc);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1ec3);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x1344);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, initialize ? 0 : data.read_word(0x1343));
	m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x4101);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0xffac);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_phase = initialize ? 302000 : 302001 + frame;
	m_check_timer->adjust(attotime::from_msec(20));
	if (!initialize && frame == 8 && rom4_codec_is("efr-loss-observe") && machine().options().verbose())
		trace_rom4_fr_input(302009,0xc8dc);
	if (!initialize) save_rom4_codec_frame(frame);
}

void tms320c54x_test_state::observe_rom4_efr_state(offs_t address, u16 opcode)
{
	if (m_rom4_codec_replay || m_phase < 302001 || m_phase > 302163) return;
	unsigned const frame = m_phase - 302001;
	auto const disable = machine().disable_side_effects();
	auto &data = m_cpu->space(AS_DATA);
	auto words = [&](char const *kind, unsigned source, unsigned count) {
		std::string values;
		for (unsigned i = 0; i < count; ++i)
		{
			u16 const value = data.read_word(source + i);
			for (int shift = 12; shift >= 0; shift -= 4) values += "0123456789abcdef"[(value >> shift) & 15];
		}
		osd_printf_info("EFRSTATE frame=%u kind=%s values=%s\n", frame, kind, values.c_str());
	};
	if (address == 0x28ee || address == 0x28f1)
	{
		bool const before = address == 0x28ee;
		expect(opcode == (before ? 0x76f8 : 0x68f8), "EFR flag observation brackets the original recovery clear");
		if (before)
			expect(m_cpu->space(AS_PROGRAM).read_word(0x28ef) == 0x0ab9 && m_cpu->space(AS_PROGRAM).read_word(0x28f0) == 0,
				"Original recovery wrapper explicitly stores zero to previous-bad-frame state");
		osd_printf_info("EFRFLAG frame=%u stage=%s value=%04x\n", frame, before ? "before" : "after", unsigned(data.read_word(0xab9)));
	}
	if (frame == 8 && address == 0xc38c && m_rom4_fr_trace_count++ < 16)
		osd_printf_info("EFRADDM frame=8 owner=%04x opcode=%04x ar4=%04x value=%04x\n", unsigned(address), opcode,
			unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR4)), unsigned(data.read_word(m_cpu->state_int(tms320c54x_device::STATE_AR4))));
	if (address == 0xc7a0)
	{
		expect(opcode == 0xe801, "EFR LSF observation is the original post-decoder instruction");
		words("R2", 0x0b80, 10); words("LSF", 0x0b94, 10);
	}
	if (address == 0xc907)
	{
		expect(opcode == 0x7215, "EFR raw synthesis observation precedes original postprocessing");
		words("RAW", 0x1388, 160); words("LPC", 0x1317, 44);
	}
	if (address == 0xc4d0)
	{
		expect(opcode == 0x1039 && (m_cpu->state_int(tms320c54x_device::STATE_ST0) & 0x1ff) == 0x15,
			"EFR code-gain clamp reads the original previous-bad-frame field through DP");
		osd_printf_info("EFRCANDIDATE frame=%u subframe=%u prev_bf=%04x previous_gain=%04x value=%04x\n",
			frame, m_rom4_efr_code_observations, unsigned(data.read_word(0xab9)), unsigned(data.read_word(0xabe)),
			unsigned((m_cpu->state_int(tms320c54x_device::STATE_B) >> 16) & 0xffff));
	}
	if (address == 0xc809 || address == 0xc85a)
	{
		bool const pitch = address == 0xc809;
		expect(opcode == (pitch ? 0x61f8 : 0x1149), "EFR gain observation follows the original gain-decoder call");
		unsigned &subframe = pitch ? m_rom4_efr_pitch_observations : m_rom4_efr_code_observations;
		expect(subframe < 4, "EFR observer records exactly four gain results per frame");
		// These routines publish gains in RAM, not an accumulator return ABI.
		osd_printf_info("EFRGAIN frame=%u kind=%s subframe=%u value=%04x\n", frame,
			pitch ? "pitch" : "code", subframe++, unsigned(data.read_word(pitch ? 0x1303 : 0x1302)));
	}
}

void tms320c54x_test_state::start_rom4_efr_packing(unsigned vector, unsigned stage)
{
	auto &data = m_cpu->space(AS_DATA);
	u16 const base = stage ? 0x1600 : 0x1400;
	u16 parameters[57];
	rom4_efr_parameters(vector, parameters);
	for (unsigned i = 0; i < 58; ++i) data.write_word(base + i, stage ? 0xdead : i < 57 ? parameters[i] : 0xdead);
	data.write_word(base - 1, 0x1357); data.write_word(base + 58, 0x2468);
	auto const packed = rom4_efr_packed(vector);
	for (unsigned i = 0; i < 17; ++i)
		data.write_word(0x1500 + i, stage ? packed[i] ^ ((stage == 2 && !i) ? 0x8000 : 0) : 0xdead);
	data.write_word(0x14ff, 0x89ab); data.write_word(0x1511, 0xcdef);
	data.write_word(0x09ba, 1); // Ordinary speech fixture, not SID/DTX transformation.
	memregion("dspprg")->as_u16(0x0f00) = 0xf4e1;
	data.write_word(0x1ec3, 0x0f00);
	m_cpu->set_state_int(tms320c54x_device::STATE_PC, stage ? 0xc2b7 : 0xc28f);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1ec3);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR2, base);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x1500);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x12345678);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xdeadbeef);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x4101);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0xffac);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_phase = 301000 + vector * 3 + stage;
	m_check_timer->adjust(attotime::from_msec(1));
}

void tms320c54x_test_state::start_rom4_efr_crc(unsigned index)
{
	auto &data = m_cpu->space(AS_DATA);
	u16 const base = index & 1 ? 0x1600 : 0x1400;
	u16 const output = base + 0x100;
	u16 parameters[57];
	rom4_efr_parameters(index / 2, parameters);
	for (unsigned i = 0; i < 57; ++i) data.write_word(base + i, parameters[i]);
	data.write_word(base - 1, 0x1357); data.write_word(base + 57, 0x2468);
	data.write_word(output - 1, 0x89ab); data.write_word(output, 0xdead); data.write_word(output + 1, 0xcdef);
	memregion("dspprg")->as_u16(0x0f00) = 0xf4e1;
	data.write_word(0x1ec3, 0x0f00);
	m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0xc2f2);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1ec3);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR2, base);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR3, output);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x12345678);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xdeadbeef);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x4101);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0xffac);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_phase = 300000 + index;
	m_check_timer->adjust(attotime::from_msec(1));
}

void tms320c54x_test_state::start_rom4_fr_packing(unsigned vector, unsigned stage)
{
	auto &data = m_cpu->space(AS_DATA);
	if (!stage)
	{
		u16 parameters[76];
		rom4_fr_parameters(vector, parameters);
		for (unsigned i = 0; i < 76; ++i) data.write_word(0x1400 + i, parameters[i]);
		for (unsigned i = 0; i < 17; ++i) data.write_word(0x1500 + i, 0xdead);
	}
	memregion("dspprg")->as_u16(0x0f00) = 0xf4e1;
	data.write_word(0x1ec3, 0x0f00);
	m_cpu->set_state_int(tms320c54x_device::STATE_PC, stage == 0 ? 0x80e7 : stage == 1 ? 0x812f : 0x80ff);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1ec3);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x1400);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x1500);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0x12345678);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xdeadbeef);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x4101);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0xffac);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_phase = 71000 + vector * 3 + stage;
	m_check_timer->adjust(attotime::from_msec(1));
}

void tms320c54x_test_state::save_rom4_codec_frame(unsigned frame)
{
	m_rom4_codec_replay = false;
	if (!frame) m_rom4_codec_replays = 0;
	m_rom4_checkpoint_pc = m_cpu->state_int(tms320c54x_device::STATE_PC);
	m_rom4_checkpoint_data.resize(0x10000 - 0x60);
	{
		auto const disable = machine().disable_side_effects();
		for (unsigned i = 0x60; i < 0x10000; ++i) m_rom4_checkpoint_data[i - 0x60] = m_cpu->space(AS_DATA).read_word(i);
	}
	m_saved_pipeline.str({}); m_saved_pipeline.clear();
	expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save native codec frame with its existing history and prepared input");
}

bool tms320c54x_test_state::replay_rom4_codec_frame(unsigned phase, unsigned milliseconds)
{
	// The reference codec and oracle remain outside the machine snapshot:
	// replay must not advance the independent reference a second time.
	auto const disable = machine().disable_side_effects();
	auto &data = m_cpu->space(AS_DATA);
	if (!m_rom4_codec_replay)
	{
		m_rom4_replay_data.resize(0x10000 - 0x60);
		for (unsigned i = 0x60; i < 0x10000; ++i) m_rom4_replay_data[i - 0x60] = data.read_word(i);
		m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never);
		expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore native codec frame without reconstructing its histories");
		expect(!m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
			m_cpu->state_int(tms320c54x_device::STATE_PC) == m_rom4_checkpoint_pc &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1ec3,
			"Codec restore returns to the live frame entry, not the completed IDLE result");
		for (unsigned i = 0x60; i < 0x10000; ++i)
			expect(data.read_word(i) == m_rom4_checkpoint_data[i - 0x60], "Codec restore recovers every pre-frame RAM word before re-execution");
		m_phase = phase; m_rom4_codec_replay = true;
		m_check_timer->adjust(attotime::from_msec(milliseconds));
		return true;
	}
	for (unsigned i = 0x60; i < 0x10000; ++i)
		expect(data.read_word(i) == m_rom4_replay_data[i - 0x60], "Native codec replay reproduces every ordinary RAM word, including histories");
	++m_rom4_codec_replays;
	return false;
}

void tms320c54x_test_state::trace_rom4_fr_state(char const *phase, offs_t address, u16 opcode)
{
	logerror("fr_trace_%s: pc=%06x op=%04x a=%010llx b=%010llx t=%04x st0=%04x st1=%04x sp=%04x ar0=%04x ar1=%04x ar2=%04x ar3=%04x ar4=%04x ar5=%04x ar6=%04x ar7=%04x bk=%04x brc=%04x\n",
		phase, unsigned(address), opcode, (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_A), (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_B),
		unsigned(m_cpu->state_int(tms320c54x_device::STATE_T)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_ST0)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_ST1)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)),
		unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR0)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR1)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR2)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR3)),
		unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR4)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR5)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR6)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_AR7)),
		unsigned(m_cpu->state_int(tms320c54x_device::STATE_BK)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_BRC)));
}

void tms320c54x_test_state::trace_rom4_fr_input(unsigned phase, u16 entry)
{
	auto &data = m_cpu->space(AS_DATA); m_rom4_fr_trace_count = 0;
	trace_rom4_fr_state("input",entry,0);
	{
		auto const disable = machine().disable_side_effects();
		for (unsigned at = 0x60; at < 0x10000; ++at) logerror("fr_trace_input_word: address=%04x value=%04x\n",at,data.read_word(at));
	}
	m_pipeline_operand_tap = data.install_read_tap(0,0xffff,"fr_frame_read",[this,phase](offs_t at,u16 &value,u16 mask) {
		if (machine().side_effects_disabled() || !m_cpu->executing() || m_phase != phase || m_rom4_codec_replay) return;
		logerror("fr_trace_read: address=%04x value=%04x mask=%04x pc_context=%04x\n",unsigned(at),value,mask,unsigned(m_cpu->state_int(tms320c54x_device::STATE_PC)));
	});
	m_pipeline_write_tap = data.install_write_tap(0,0xffff,"fr_frame_write",[this,phase](offs_t at,u16 &value,u16 mask) {
		if (machine().side_effects_disabled() || !m_cpu->executing() || m_phase != phase || m_rom4_codec_replay) return;
		logerror("fr_trace_write: address=%04x value=%04x mask=%04x pc_context=%04x\n",unsigned(at),value,mask,unsigned(m_cpu->state_int(tms320c54x_device::STATE_PC)));
	});
}

void tms320c54x_test_state::start_rom4_fr_decoder(unsigned frame, bool initialize)
{
	auto &data = m_cpu->space(AS_DATA);
	expect(memregion("dspprg")->as_u16(0x8449) == 0xf074 && memregion("dspprg")->as_u16(0x844a) == 0x23c4,
		"FR decoder fixture has the acquired program loaded, not an empty BIOS region");
	if (initialize)
	{
		m_rom4_codec.reset();
		if (!frame) m_rom4_pcm_mismatches = 0;
	}
	else
	{
		util::gsm_fr_codec::pcm_block input{};
		u32 random = 0x12345678U ^ frame;
		for (unsigned i = 0; i < input.size(); ++i)
		{
			random = random * 1664525U + 1013904223U;
			switch (frame / 8)
			{
			case 0: input[i] = 0; break;
			case 1: input[i] = i == frame % 160 ? 32767 : 0; break;
			case 2: input[i] = i & 1 ? 12000 : -12000; break;
			case 3: input[i] = s16(random >> 16); break;
			}
		}
		util::gsm_fr_codec::speech_frame encoded{};
		expect(m_rom4_codec.encode(input, encoded) && m_rom4_codec.decode(encoded, m_rom4_pcm), "FR reference codec available");
		gsm::tch_f::speech_bits serial{};
		expect(gsm::tch_f::unpack_speech(encoded, serial), "FR reference serial frame");
		unsigned position = 0;
		for (unsigned p = 0; p < 76; ++p)
		{
			u16 value = 0;
			for (unsigned bit = 0; bit < ROM4_FR_WIDTHS[p]; ++bit) value = (value << 1) | serial[position++];
			data.write_word(0x1390 + p, value);
		}
		for (unsigned i = 0; i < 160; ++i) data.write_word(0x1500 + i, 0xdead);
	}
	memregion("dspprg")->as_u16(0x0f00) = 0xf4e1;
	data.write_word(0x1ec3, 0x0f00);
	m_cpu->set_state_int(tms320c54x_device::STATE_PC, initialize ? 0x8449 : 0x8458);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1ec3);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x1500);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x4101);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0xffac);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_phase = 72000 + frame * 2 + (initialize ? 0 : 1);
	m_check_timer->adjust(attotime::from_msec(5));
	if (!initialize) save_rom4_codec_frame(frame);
	if (!initialize && frame == 8 && machine().options().verbose())
		trace_rom4_fr_input(72017,0x8458);
}

void tms320c54x_test_state::start_rom4_fr_encoder(unsigned frame, unsigned stage)
{
	auto &data = m_cpu->space(AS_DATA);
	expect(memregion("dspprg")->as_u16(0x8045) == 0xf074 && memregion("dspprg")->as_u16(0x8046) == 0x23c6,
		"FR encoder fixture has the acquired program loaded, not an empty BIOS region");
	if (!stage)
	{
		m_rom4_codec.reset();
		if (!frame) m_rom4_pcm_mismatches = 0;
	}
	if (stage == 1)
	{
		util::gsm_fr_codec::pcm_block input{};
		u32 random = 0x12345678U ^ frame;
		for (unsigned i = 0; i < input.size(); ++i)
		{
			random = random * 1664525U + 1013904223U;
			switch (frame / 8)
			{
			case 0: input[i] = 0; break;
			case 1: input[i] = i == frame % 160 ? 32767 : 0; break;
			case 2: input[i] = i & 1 ? 12000 : -12000; break;
			case 3: input[i] = s16(random >> 16); break;
			}
			data.write_word(0x1500 + i, u16(input[i]));
		}
		util::gsm_fr_codec::speech_frame encoded{};
		expect(m_rom4_codec.encode(input, encoded), "FR encoder reference available");
		gsm::tch_f::speech_bits serial{};
		expect(gsm::tch_f::unpack_speech(encoded, serial), "FR encoder reference serial frame");
		unsigned position = 0;
		for (unsigned p = 0; p < 76; ++p)
		{
			u16 value = 0;
			for (unsigned bit = 0; bit < ROM4_FR_WIDTHS[p]; ++bit) value = (value << 1) | serial[position++];
			m_rom4_parameters[p] = value;
		}
	}
	memregion("dspprg")->as_u16(0x0f00) = 0xf4e1;
	data.write_word(0x1ec3, 0x0f00);
	m_cpu->set_state_int(tms320c54x_device::STATE_PC, stage == 0 ? 0x8045 : stage == 1 ? 0x85e0 : 0x860c);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1ec3);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR4, 0x1500);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x4101);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0xffac);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_phase = 74000 + frame * 3 + stage;
	m_check_timer->adjust(attotime::from_msec(5));
	if (stage == 1) save_rom4_codec_frame(frame);
	if (!frame && stage == 2 && !m_rom4_codec_replay && machine().options().verbose()) trace_rom4_fr_input(74002,0x860c);
}

void tms320c54x_test_state::start_rom4_pcm_mute(unsigned index)
{
	auto &data = m_cpu->space(AS_DATA);
	static constexpr u16 flags[] = { 0x0000, 0x0001, 0xfffe, 0xffff };
	u16 const base = index < 4 ? 0x1500 : 0x1600;
	data.write_word(base - 1, 0x1357);
	data.write_word(base + 160, 0x2468);
	for (unsigned i = 0; i < 160; ++i)
		data.write_word(base + i, u16(0x8001U + i * 257U));
	data.write_word(0x1f2b, flags[index % 4]);
	memregion("dspprg")->as_u16(0x0f00) = 0xf4e1;
	data.write_word(0x1ec3, 0x0f00);
	m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0xa0b8);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1ec3);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR2, base);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x4101);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0xffac);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_phase = 74100 + index;
	m_check_timer->adjust(attotime::from_msec(5));
}

std::array<s16, 4> tms320c54x_test_state::rom4_pair_vector(unsigned index)
{
	static constexpr s16 vectors[8][4] = {
		{0,0,0,0}, {1,0,1,0}, {1,0,0,1}, {0,1,1,0},
		{123,-456,789,321}, {-16384,8192,4096,-2048},
		{16384,-8192,-4096,2048}, {32767,0,1,-1}
	};
	return {vectors[index/4][0], vectors[index/4][1], vectors[index/4][2], vectors[index/4][3]};
}

void tms320c54x_test_state::start_rom4_pairs(unsigned index)
{
	auto &data = m_cpu->space(AS_DATA);
	static constexpr u16 witness[] = {0x7712,0x1254,0x7713,0x1255,0x308a,0x218a,0x2082,0x3083,
		0xfa30,0x47f3,0x2d92,0x2882,0xfc00,0xfe00,0xe554,0xe510};
	for (unsigned i=0; i<std::size(witness); ++i)
		expect(memregion("dspprg")->as_u16(0x47e6+i)==witness[i], "Original adjacent-pair helper is unchanged");
	auto const values=rom4_pair_vector(index);
	for (unsigned i=0; i<4; ++i) data.write_word(0x1252+i, u16(values[i]));
	data.write_word(0x1251,0xa55a); data.write_word(0x1256,0x5aa5);
	// Isolated algorithm inputs and caller return, never a handset state hook.
	memregion("dspprg")->as_u16(0x0f00)=0xf4e1;
	data.write_word(0x1ec3,0x0f00);
	m_cpu->set_state_int(tms320c54x_device::STATE_PC,0x47e6);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP,0x1ec3);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0,0x081f | (index&1 ? 0x1000 : 0));
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1,0x0900 | (index&2 ? 0x40 : 0));
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST,0xffac);
	m_cpu->set_state_int(tms320c54x_device::STATE_A,0x123456);
	m_cpu->set_state_int(tms320c54x_device::STATE_B,0xabcdef);
	m_cpu->set_state_int(tms320c54x_device::STATE_T,0xa55a);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE,0);
	m_phase=711000+index;
	m_check_timer->adjust(attotime::from_msec(1));
	save_rom4_codec_frame(index);
}

bool tms320c54x_test_state::rom4_bitmap_bit(unsigned index, unsigned bit)
{
	return index < 512 ? bit == index : index == 513 || (index == 514 && !(bit & 1));
}

void tms320c54x_test_state::start_rom4_bitmap(unsigned index)
{
	auto &data = m_cpu->space(AS_DATA);
	for (unsigned i = 0; i < 514; ++i) data.write_word(0x0fff + i, 0xa55a);
	for (unsigned word = 0; word < 32; ++word)
	{
		u16 value = 0;
		for (unsigned bit = 0; bit < 16; ++bit)
			if (rom4_bitmap_bit(index, word * 16 + bit)) value |= 1U << bit;
		data.write_word(0x141f - word, value);
	}
	// Isolated algorithm caller: only this return sentinel is fixture code.
	memregion("dspprg")->as_u16(0x0f00) = 0xf4e1;
	expect(memregion("dspprg")->as_u16(0x2428) == 0xfc00 &&
		memregion("dspprg")->as_u16(0x48f7) == 0xf074,
		"Acquired bitmap entry and optional hook are unchanged");
	data.write_word(0x1ec3, 0x0f00);
	m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x48f7);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1ec3);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR2, 0x141f);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR3, 0x1000);
	m_cpu->set_state_int(tms320c54x_device::STATE_AR7, 0x55aa);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x4101);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0xffac);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_phase = 710000 + index;
	m_check_timer->adjust(attotime::from_msec(1));
	save_rom4_codec_frame(index);
}

void tms320c54x_test_state::start_rom4_crc_helper(unsigned index)
{
	// Only the return sentinel is fixture code; 8179..8180 stays acquired ROM.
	memregion("dspprg")->as_u16(0x0f00) = 0xf4e1;
	m_cpu->space(AS_DATA).write_word(0x1ec3, 0x0f00);
	static constexpr u32 seeds[] = { 0, 1, 7, 0x80000000, 0xffffffff,
		0x12345678, 0xaaaaaaaa, 0x55555555 };
	m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x8179);
	m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x1ec3);
	m_cpu->set_state_int(tms320c54x_device::STATE_A, seeds[index / 64]);
	m_cpu->set_state_int(tms320c54x_device::STATE_B, 0xdeadbeef);
	m_cpu->set_state_int(tms320c54x_device::STATE_BRC, (index / 2) % 32);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST0, index & 1 ? 0x0800 : 0);
	m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x4101);
	m_cpu->set_state_int(tms320c54x_device::STATE_PMST, 0xffac);
	m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
	m_phase = 63000 + index;
	m_check_timer->adjust(attotime::from_usec(100));
}

u16 tms320c54x_test_state::rom4_loader_entry_r()
{
	const u16 word = memregion("dspprg")->as_u16(0x0f00);
	if (m_phase == rom4_staged_cold_phase && !machine().side_effects_disabled())
	{
		expect(word == 0, "Staged ROM4 cold execution reaches the unpopulated loader fetch");
		osd_printf_info("TMS320C54x ROM4 staged cold frontier: fetch=000f00 pc=%04x sp=%04x pmst=%04x cycles=%llu\n",
			u16(m_cpu->state_int(tms320c54x_device::STATE_PC)), u16(m_cpu->state_int(tms320c54x_device::STATE_SP)),
			u16(m_cpu->state_int(tms320c54x_device::STATE_PMST)), (unsigned long long)m_cpu->pipeline_cycles());
		throw emu_fatalerror(0, "TMS320C54x ROM4 staged cold frontier complete");
	}
	if (m_phase == 4 && !machine().side_effects_disabled())
	{
		// Stop at the absent loader's fetch, not at an unsupported opcode:
		// zero is a valid ADD instruction and must not serve as a sentinel.
		expect(word == 0 && m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x0f01,
			"ROM4 cold fetch reaches the unpopulated MCU-uploaded loader");
		osd_printf_info("TMS320C54x ROM4 cold frontier: pc=%04x sp=%04x pmst=%04x idle=%u\n",
			u16(m_cpu->state_int(tms320c54x_device::STATE_PC)),
			u16(m_cpu->state_int(tms320c54x_device::STATE_SP)),
			u16(m_cpu->state_int(tms320c54x_device::STATE_PMST)),
			unsigned(m_cpu->state_int(tms320c54x_device::STATE_IDLE)));
		throw emu_fatalerror(0, "TMS320C54x ROM4 cold frontier complete");
	}
	return word;
}

bool tms320c54x_test_state::check_rom4_fr_decoder_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase >= 72000 && m_phase < 72064)
	{
		unsigned const frame = (m_phase - 72000) / 2;
		bool const initialize = !(m_phase & 1);
		if (!m_cpu->state_int(tms320c54x_device::STATE_IDLE) || m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) ||
			m_cpu->state_int(tms320c54x_device::STATE_PC) != 0x0f01 || m_cpu->state_int(tms320c54x_device::STATE_SP) != 0x1ec4)
			osd_printf_info("ROM4 FR decoder stop: frame=%u initialize=%u pc=%04x illegal=%u sp=%04x\n",
				frame, initialize, unsigned(m_cpu->state_int(tms320c54x_device::STATE_PC)),
				unsigned(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL)),
				unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)));
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
			m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x0f01 &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1ec4,
			"native FR decoder/reset returns with balanced stack");
		if (initialize) { start_rom4_fr_decoder(frame, false); return true; }
		unsigned differences = 0;
		for (unsigned i = 0; i < 160; ++i)
		{
			s16 const actual = s16(m_cpu->space(AS_DATA).read_word(0x1500 + i));
			if (actual != m_rom4_pcm[i])
			{
				if (!differences) osd_printf_info("ROM4 FR decoder mismatch: frame=%u sample=%u actual=%d expected=%d\n", frame, i, actual, m_rom4_pcm[i]);
				++differences;
			}
		}
		if (replay_rom4_codec_frame(72000 + frame * 2 + 1)) return true;
		m_rom4_pcm_mismatches += differences;
		osd_printf_info("ROM4 FR decoder comparison: frame=%u mismatches=%u\n", frame, differences);
		if (frame < 31) { start_rom4_fr_decoder(frame + 1, (frame + 1) % 8 == 0); return true; }
		osd_printf_info("TMS320C54x ROM4 FR decoder comparison: frames=32 samples=5120 mismatches=%u\n", m_rom4_pcm_mismatches);
		expect(!m_rom4_pcm_mismatches, "native FR decoder PCM matches independent codec");
		expect(m_rom4_codec_replays == 32, "Every native FR decoder frame is replayed");
		osd_printf_info("TMS320C54x ROM4 FR decoder replay: PASS frames=32 ram_words=65440 checkpoints=frame_input reference_advance=once handset_audio_claim=0\n");
		osd_printf_info("TMS320C54x ROM4 FR decoder: PASS frames=32 samples=5120 handset_audio_claim=0\n");
		throw emu_fatalerror(0, "TMS320C54x ROM4 FR decoder complete");
	}
	if (m_phase >= 74000 && m_phase < 74096)
	{
		unsigned const frame = (m_phase - 74000) / 3;
		unsigned const stage = (m_phase - 74000) % 3;
		if (!m_cpu->state_int(tms320c54x_device::STATE_IDLE) || m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) ||
			m_cpu->state_int(tms320c54x_device::STATE_PC) != 0x0f01 || m_cpu->state_int(tms320c54x_device::STATE_SP) != 0x1ec4)
		{
			osd_printf_info("ROM4 FR encoder stop: frame=%u stage=%u pc=%04x sp=%04x illegal=%u\n", frame, stage,
				unsigned(m_cpu->state_int(tms320c54x_device::STATE_PC)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL)));
			if (machine().options().verbose()) trace_rom4_fr_state("encoder_stop",m_cpu->state_int(tms320c54x_device::STATE_PC),0);
		}
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
			m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x0f01 && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1ec4,
			"native FR encoder stage returns with balanced stack");
		if (stage < 2) { start_rom4_fr_encoder(frame, stage + 1); return true; }
		auto const reference_state = m_rom4_codec.snapshot().channels[0];
		// Native L_z2 is split low/high at 0x1001/2, unlike the reference scalar.
		expect(s16(data.read_word(0x1000)) == reference_state.z1 &&
			data.read_word(0x1001) == (reference_state.l_z2 & 0x7fff) &&
			s16(data.read_word(0x1002)) == (reference_state.l_z2 >> 15) &&
			s16(data.read_word(0x1003)) == reference_state.mp,
			"native FR encoder preprocessing and preemphasis histories match reference");
		unsigned differences = 0;
		for (unsigned p = 0; p < 76; ++p)
		{
			u16 const actual = m_cpu->space(AS_DATA).read_word(0x1390 + p);
			if (actual != m_rom4_parameters[p])
			{
				if (!differences) osd_printf_info("ROM4 FR encoder mismatch: frame=%u parameter=%u actual=%u expected=%u\n", frame, p, actual, m_rom4_parameters[p]);
				++differences;
			}
		}
		if (replay_rom4_codec_frame(74000 + frame * 3 + 1)) return true;
		m_rom4_pcm_mismatches += differences;
		osd_printf_info("ROM4 FR encoder comparison: frame=%u mismatches=%u\n", frame, differences);
		if (frame < 31) { start_rom4_fr_encoder(frame + 1, (frame + 1) % 8 == 0 ? 0 : 1); return true; }
		osd_printf_info("ROM4 FR encoder aggregate: frames=32 parameters=2432 mismatches=%u\n", m_rom4_pcm_mismatches);
		expect(!m_rom4_pcm_mismatches, "native FR encoder parameters match independent codec");
		expect(m_rom4_codec_replays == 32, "Every native FR encoder frame is replayed");
		osd_printf_info("TMS320C54x ROM4 FR encoder replay: PASS frames=32 ram_words=65440 checkpoints=before_preprocessing reference_advance=once handset_audio_claim=0\n");
		osd_printf_info("TMS320C54x ROM4 FR encoder: PASS frames=32 parameters=2432 handset_audio_claim=0\n");
		start_rom4_pcm_mute(0);
		return true;
	}
	if (m_phase >= 74100 && m_phase < 74108)
	{
		unsigned const index = m_phase - 74100;
		u16 const base = index < 4 ? 0x1500 : 0x1600;
		bool const muted = index & 1;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
			m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x0f01 &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1ec4,
			"native conditional PCM mute returns with balanced stack");
		for (unsigned i = 0; i < 160; ++i)
			expect(data.read_word(base + i) == (muted ? 0 : u16(0x8001U + i * 257U)),
				"native conditional PCM mute preserves or zeros each sample");
		expect(data.read_word(base - 1) == 0x1357 && data.read_word(base + 160) == 0x2468,
			"native conditional PCM mute does not overwrite adjacent words");
		expect(m_cpu->state_int(tms320c54x_device::STATE_AR2) == base + (muted ? 160 : 0),
			"native conditional PCM mute advances only the muted buffer");
		if (index < 7) { start_rom4_pcm_mute(index + 1); return true; }
		osd_printf_info("TMS320C54x ROM4 PCM mute: PASS cases=8 samples=1280 guards=16 handset_audio_claim=0\n");
		throw emu_fatalerror(0, "TMS320C54x ROM4 FR encoder complete");
	}
	if (m_phase >= 303000 && m_phase < 303010)
	{
		unsigned const index = m_phase - 303000;
		unsigned const count = std::array<unsigned, 5>{0, 1, 2, 9, 39}[index / 2];
		bool const returning = index & 1;
		unsigned const body = returning ? 0x0e02 : 0x0e03;
		if (!m_rom4_codec_replays && m_cpu->state_int(tms320c54x_device::STATE_PC) == body &&
			m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x1400)
		{
			m_saved_pipeline.str({}); m_saved_pipeline.clear();
			expect(machine().save().write_stream(m_saved_pipeline) == STATERR_NONE, "Save pending delayed control and armed RPT before its body");
			m_rom4_codec_replays = 1;
		}
		if (!m_cpu->state_int(tms320c54x_device::STATE_IDLE))
		{
			m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
		}
		expect(m_rom4_codec_replays == 1 && m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x0e21,
			"Delayed repeat reaches its control target after an armed-state checkpoint");
		for (unsigned i = 0; i < 40; ++i)
			expect(data.read_word(0x1600 + i) == (i <= count ? u16(0x1357 + i * 257) : 0xdead),
				"RPT in delayed branch/return completes every copy before transfer");
		expect(m_cpu->state_int(tms320c54x_device::STATE_AR4) == 0x1401 + count &&
			m_cpu->state_int(tms320c54x_device::STATE_AR5) == 0x1601 + count &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == (returning ? 0x1ec4 : 0x1ec3) &&
			data.read_word(0x15ff) == 0x1234 && data.read_word(0x1628) == 0x5678,
			"Delayed repeat preserves guards and publishes complete pointers and return stack");
		if (!m_rom4_codec_replay)
		{
			m_saved_pipeline.clear(); m_saved_pipeline.seekg(0); m_check_timer->adjust(attotime::never);
			expect(machine().save().read_stream(m_saved_pipeline) == STATERR_NONE, "Restore armed RPT and pending delayed transfer");
			m_phase = 303000 + index; m_rom4_codec_replay = true;
			m_check_timer->adjust(attotime::from_ticks(1, m_cpu->clock())); return true;
		}
		if (index < 9) { start_rom4_delayed_repeat(index + 1); return true; }
		osd_printf_info("TMS320C54x ROM4 delayed repeat: PASS cases=10 replay=10 controls=BD,RETD counts=0,1,2,9,39\n");
		if(!strcmp(machine().options().bios(),"efr-decoder-pipe"))
		{
			// The existing delayed-repeat preflight is atomic; the acquired
			// decoder reset and every frame below use the staged frontend.
			m_cpu->set_pipeline_frontend(true);
			osd_printf_info("ROM4 EFR decoder frontend: staged handset_audio_claim=0\n");
		}
		start_rom4_efr_decoder(0, true); return true;
	}
	return false;
}

bool tms320c54x_test_state::check_rom4_codec_is_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase >= 304000 && m_phase < 304519)
	{
		bool const dtx = rom4_codec_is("efr-dtx-encoder");
		bool const homing = rom4_codec_is("efr-enc-homing");
		unsigned const frames = dtx ? 518 : homing ? 46 : 163;
		expect(m_phase <= 304000 + frames, "EFR encoder phase stays within the selected reference input");
		if (!m_cpu->state_int(tms320c54x_device::STATE_IDLE) || m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL))
			osd_printf_info("ROM4 EFR encoder stop: phase=%u pc=%04x sp=%04x illegal=%u\n", m_phase,
				unsigned(m_cpu->state_int(tms320c54x_device::STATE_PC)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)),
				unsigned(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL)));
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) && m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
			m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x0f01 && m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1ec4,
			"Native EFR encoder reset/frame returns with balanced stack");
		if (m_phase == 304000) { start_rom4_efr_encoder(0, false); return true; }
		unsigned const frame = m_phase - 304001;
		if (replay_rom4_codec_frame(304001 + frame, 20)) return true;
		u8 const *const reference = memregion(dtx ? "efr_dtx_encoder_serial" : homing ? "efr_homing_encoder_serial" : "efr_encoder_serial")->base() + frame * 492;
		auto word = [&](unsigned index) { return u16(reference[index * 2] | (reference[index * 2 + 1] << 8)); };
		unsigned bit = 0, mismatches = 0;
		for (unsigned p = 0; p < 57; ++p)
		{
			u16 expected = 0;
			for (unsigned i = 0; i < ROM4_EFR_WIDTHS[p]; ++i) expected = (expected << 1) | word(bit++);
			u16 const actual = data.read_word(0x0e36 + p);
			if (actual != expected)
			{
				if (mismatches < 8) osd_printf_info("ROM4 EFR parameter mismatch: frame=%u parameter=%u actual=%04x expected=%04x\n", frame, p, actual, expected);
				++mismatches;
			}
		}
		for (unsigned flag = 0; flag < 2; ++flag)
			if (BIT(data.read_word(0x09ba), flag ? 0 : 1) != word(244 + flag)) ++mismatches;
		std::array<u8, 244> speech;
		for (unsigned i = 0; i < speech.size(); ++i) speech[i] = word(i);
		auto const packed = rom4_efr_packed(speech);
		for (unsigned i = 0; i < packed.size(); ++i)
			if (data.read_word(0x1897 + i) != packed[i]) ++mismatches;
		osd_printf_info("ROM4 EFR%s encoder comparison: frame=%u mismatches=%u\n", dtx ? " DTX" : homing ? " homing" : "", frame, mismatches);
		m_rom4_pcm_mismatches += mismatches;
		expect(!mismatches, "Native EFR encoder parameters and VAD/SP match independent ETSI output");
		if (frame + 1 < frames) { start_rom4_efr_encoder(frame + 1, false); return true; }
		expect(m_rom4_codec_replays == frames, "EFR encoder replays every frame with its existing histories");
		if (homing)
		{
			osd_printf_info("TMS320C54x ROM4 EFR homing encoder replay: PASS frames=46 ram_words=65440 checkpoints=before_preprocessing handset_audio_claim=0\n");
			osd_printf_info("TMS320C54x ROM4 EFR homing encoder: PASS frames=46 parameters=2622 flags=92 packed_words=782 homing=6 repeated_speech=20 mismatches=0 handset_audio_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x ROM4 EFR homing encoder complete");
		}
		if (dtx)
		{
			osd_printf_info("TMS320C54x ROM4 EFR DTX encoder replay: PASS frames=518 ram_words=65440 checkpoints=before_preprocessing handset_audio_claim=0\n");
			osd_printf_info("TMS320C54x ROM4 EFR DTX encoder: PASS frames=518 parameters=29526 flags=1036 packed_words=8806 mismatches=0 handset_audio_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x ROM4 EFR DTX encoder complete");
		}
		osd_printf_info("TMS320C54x ROM4 EFR encoder replay: PASS frames=163 ram_words=65440 checkpoints=before_preprocessing handset_audio_claim=0\n");
		osd_printf_info("TMS320C54x ROM4 EFR encoder: PASS frames=163 parameters=9291 flags=326 packed_words=2771 mismatches=0 handset_audio_claim=0\n");
		throw emu_fatalerror(0, "TMS320C54x ROM4 EFR encoder complete");
	}
	if (m_phase >= 711000 && m_phase < 711032)
	{
		unsigned const index=m_phase-711000;
		bool const tc=index&1;
		auto const values=rom4_pair_vector(index);
		s64 const scale=index&2 ? 2 : 1;
		s64 const dot=(s64(values[2])*values[0]+s64(values[3])*values[1])*scale;
		s64 const cross=(s64(values[2])*values[1]-s64(values[3])*values[0])*scale;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
			m_cpu->state_int(tms320c54x_device::STATE_PC)==0x0f01 &&
			m_cpu->state_int(tms320c54x_device::STATE_SP)==0x1ec4,
			"Original pair helper returns with balanced caller stack");
		expect(m_cpu->state_int(tms320c54x_device::STATE_A)==(u64(dot)&0xffffffffffULL) &&
			m_cpu->state_int(tms320c54x_device::STATE_B)==(u64(cross)&0xffffffffffULL),
			"Both TC paths compute complete signed dot and cross products");
		for (unsigned i=0; i<4; ++i)
			expect(data.read_word(0x1252+i)==u16(values[i<2 && tc ? i+2 : i]),
				"Only the TC-set return copies the current pair into history");
		expect(data.read_word(0x1251)==0xa55a && data.read_word(0x1256)==0x5aa5 &&
			m_cpu->state_int(tms320c54x_device::STATE_AR2)==(tc ? 0x1252 : 0x1253) &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3)==(tc ? 0x1254 : 0x1255) &&
			m_cpu->state_int(tms320c54x_device::STATE_T)==u16(values[3]) &&
			bool(m_cpu->state_int(tms320c54x_device::STATE_ST0)&0x1000)==tc,
			"Pair history guards, pointers, T and TC agree with original operations");
		if (replay_rom4_codec_frame(711000+index,1)) return true;
		if (index<31) { start_rom4_pairs(index+1); return true; }
		expect(m_rom4_codec_replays==32,"Every original pair case replays full ordinary RAM");
		osd_printf_info("TMS320C54x ROM4 RF pairs: PASS cases=32 vectors=8 tc=2 frct=2 replay=32 ram_words=65440 dot_cross_both_tc=1 history_copy_tc_only=1 rf_acquisition_claim=0\n");
		throw emu_fatalerror(0,"TMS320C54x ROM4 RF pairs complete");
	}
	if (m_phase >= 710000 && m_phase < 710515)
	{
		unsigned const index = m_phase - 710000;
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
			m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x0f01 &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1ec4,
			"Original RF bitmap helper returns with balanced stack");
		if (replay_rom4_codec_frame(710000 + index, 1)) return true;
		unsigned count = 0;
		for (unsigned bit = 0; bit < 512; ++bit)
			if (rom4_bitmap_bit(index, bit))
			{
				u16 const expected = bit < 128 ? bit + 1 : bit + 384;
				expect(data.read_word(0x1000 + count++) == expected,
					"Original RF decoder maps each selected bit to its ordered list entry");
			}
		for (unsigned i = count; i <= 512; ++i)
			expect(data.read_word(0x1000 + i) == 0xa55a, "RF decoder preserves unwritten output and guard");
		expect(data.read_word(0x0fff) == 0xa55a &&
			(m_cpu->state_int(tms320c54x_device::STATE_B) & 0xffff) == count &&
			m_cpu->state_int(tms320c54x_device::STATE_AR3) == 0x1000 + count &&
			m_cpu->state_int(tms320c54x_device::STATE_AR2) == 0x13ff &&
			m_cpu->state_int(tms320c54x_device::STATE_AR7) == 0x55aa,
			"RF bitmap count, consumed extent, output pointer and saved AR7 agree");
		for (unsigned word = 0; word < 32; ++word)
		{
			u16 expected = 0;
			for (unsigned bit = 0; bit < 16; ++bit)
				if (rom4_bitmap_bit(index, word * 16 + bit)) expected |= 1U << bit;
			expect(data.read_word(0x141f - word) == expected, "RF bitmap input remains unchanged");
		}
		if (index < 514) { start_rom4_bitmap(index + 1); return true; }
		expect(m_rom4_codec_replays == 515, "Every native RF bitmap case replays full ordinary RAM");
		osd_printf_info("TMS320C54x ROM4 RF bitmap: PASS cases=515 basis=512 controls=empty,full,alternating replay=515 ram_words=65440 rf_acquisition_claim=0\n");
		throw emu_fatalerror(0, "TMS320C54x ROM4 RF bitmap complete");
	}
	if (m_phase >= 302000 && m_phase < 302519)
	{
		bool const dtx = rom4_codec_is("efr-dtx-decoder");
		bool const invalid_sid = rom4_codec_is("efr-invalid-sid");
		bool const lost_sid_observing = rom4_codec_is("efr-lsid-observe");
		bool const lost_sid = rom4_codec_is("efr-lost-sid") || lost_sid_observing;
		bool const observing = rom4_codec_is("efr-loss-observe");
		bool const erasure = observing || rom4_codec_is("efr-erasure");
		bool const homing = rom4_codec_is("efr-homing");
		bool const homing_errors = rom4_codec_is("efr-home-errors");
		auto const fixture = rom4_efr_decoder_fixture();
		unsigned const frames = fixture.frames;
		expect(m_phase <= 302000 + frames, "EFR decoder phase stays within the selected reference input");
		if (!m_cpu->state_int(tms320c54x_device::STATE_IDLE) || m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL))
			osd_printf_info("ROM4 EFR decoder stop: phase=%u pc=%04x sp=%04x illegal=%u\n", m_phase,
				unsigned(m_cpu->state_int(tms320c54x_device::STATE_PC)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)),
				unsigned(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL)));
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
			m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x0f01 &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1ec4,
			"native EFR decoder reset/frame returns with balanced stack");
		if (m_phase == 302000) { start_rom4_efr_decoder(0, false); return true; }
		unsigned const frame = m_phase - 302001;
		if (replay_rom4_codec_frame(302001 + frame, 20)) return true;
		u8 const *const reference = memregion(fixture.pcm)->base() + frame * 320;
		unsigned mismatches = 0;
		bool nonzero = false;
		std::string observed_pcm;
		for (unsigned i = 0; i < 160; ++i)
		{
			u16 const expected = reference[i * 2] | (reference[i * 2 + 1] << 8);
			u16 const actual = data.read_word(0x1388 + i);
			if (observing || homing_errors || lost_sid_observing)
				for (int shift = 12; shift >= 0; shift -= 4) observed_pcm += "0123456789abcdef"[(actual >> shift) & 15];
			if (homing && (frame == 0 || frame == 1 || frame == 23 || frame == 45))
				expect(actual == 8 && expected == 8, "steady decoder homing publishes the EHF pattern");
			nonzero |= actual != 0;
			if (actual != expected)
			{
				if (mismatches < 8) osd_printf_info("ROM4 EFR PCM mismatch: frame=%u sample=%u actual=%04x expected=%04x\n", frame, i, actual, expected);
				++mismatches;
			}
		}
		bool const unsettled_homing = homing_errors && (frame <= 1 || (frame >= 23 && frame <= 24) || (frame >= 46 && frame <= 47));
		if (homing_errors)
		{
			osd_printf_info("ROM4 EFR homing error frame: frame=%u mismatches=%u resets=%u reference_settled=%u pcm=%s\n",
				frame, mismatches, m_rom4_homing_resets, unsigned(!unsettled_homing), observed_pcm.c_str());
		}
		else
			osd_printf_info("ROM4 EFR%s decoder comparison: frame=%u mismatches=%u\n", fixture.label, frame, mismatches);
		m_rom4_pcm_mismatches += mismatches;
		if (observing)
			osd_printf_info("ROM4 EFR loss observation: frame=%u mismatches=%u pcm=%s\n", frame, mismatches, observed_pcm.c_str());
		else if (lost_sid_observing)
			osd_printf_info("ROM4 EFR lost SID observation: frame=%u mismatches=%u pcm=%s\n", frame, mismatches, observed_pcm.c_str());
		else if (!homing_errors)
			expect(!mismatches, "native EFR decoder matches independent ETSI PCM");
		if (invalid_sid)
		{
			u8 const *const input = memregion(fixture.input)->base() + frame * 494;
			bool const invalid = input[490] == 1 || (input[490] == 2 && input[0]);
			expect(bool(data.read_word(0x09c0) & 0x20) == invalid,
				"Original EFR RX handler marks invalid codewords and bad accepted SID frames");
		}
		if (lost_sid && (frame == 95 || frame == 96 || frame == 97 || frame == 356))
		{
			u16 const expected = frame == 95 ? 0x0290 : frame == 96 ? 0x02c0 : frame == 97 ? 0x0501 : 0x0401;
			if (lost_sid_observing)
				osd_printf_info("ROM4 EFR lost SID flags: frame=%u native=%04x reference=%04x\n", frame, data.read_word(0x09c0), expected);
			else
				expect(data.read_word(0x09c0) == expected, "Original EFR lost-SID muting and first-speech flags match reference");
		}
		if (dtx)
		{
			u8 const *const input = memregion("efr_dtx_input")->base() + frame * 494;
			if ((input[0] || input[490] == 2) && nonzero) ++m_rom4_cni_nonzero_frames;
		}
		if (frame + 1 < frames) { start_rom4_efr_decoder(frame + 1, false); return true; }
		expect(m_rom4_codec_replays == frames, "EFR replays every frame with its original codec histories");
		if (homing_errors)
		{
			osd_printf_info("TMS320C54x ROM4 EFR homing error replay: COMPLETE frames=49 ram_words=65440 checkpoints=frame_input handset_audio_claim=0\n");
			osd_printf_info("TMS320C54x ROM4 EFR homing error observation: COMPLETE frames=49 samples=7840 conformance_claim=0 handset_audio_claim=0\n");
			osd_printf_info("ROM4 EFR homing error total: mismatches=%u\n", m_rom4_pcm_mismatches);
			throw emu_fatalerror(0, "TMS320C54x ROM4 EFR homing error complete");
		}
		if (lost_sid)
		{
			if (lost_sid_observing)
			{
				osd_printf_info("TMS320C54x ROM4 EFR lost SID observation replay: COMPLETE frames=518 ram_words=65440 checkpoints=frame_input\n");
				osd_printf_info("TMS320C54x ROM4 EFR lost SID observation: COMPLETE frames=518 samples=82880 lost_sid=2 mismatches=%u conformance_claim=0 handset_audio_claim=0\n", m_rom4_pcm_mismatches);
				throw emu_fatalerror(0, "TMS320C54x ROM4 EFR lost SID observation complete");
			}
			osd_printf_info("TMS320C54x ROM4 EFR lost SID decoder replay: PASS frames=518 ram_words=65440 checkpoints=frame_input handset_audio_claim=0\n");
			osd_printf_info("TMS320C54x ROM4 EFR lost SID decoder: PASS frames=518 samples=82880 lost_sid=2 mismatches=0 handset_audio_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x ROM4 EFR lost SID decoder complete");
		}
		if (invalid_sid)
		{
			u8 const *const input = memregion(fixture.input)->base();
			unsigned invalid = 0, bad_valid = 0;
			for (unsigned f = 0; f < frames; ++f)
			{
				invalid += input[f * 494 + 490] == 1;
				bad_valid += input[f * 494 + 490] == 2 && input[f * 494];
			}
			expect(invalid == 4 && bad_valid == 1, "Invalid-SID fixture exercises both accepted bad-frame categories");
			osd_printf_info("TMS320C54x ROM4 EFR invalid SID decoder replay: PASS frames=518 ram_words=65440 checkpoints=frame_input handset_audio_claim=0\n");
			osd_printf_info("TMS320C54x ROM4 EFR invalid SID decoder: PASS frames=518 samples=82880 invalid_codewords=4 bad_valid_codewords=1 control_checked=518 mismatches=0 handset_audio_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x ROM4 EFR invalid SID decoder complete");
		}
		if (homing)
		{
			osd_printf_info("TMS320C54x ROM4 EFR homing decoder replay: PASS frames=46 ram_words=65440 checkpoints=frame_input handset_audio_claim=0\n");
			osd_printf_info("TMS320C54x ROM4 EFR homing decoder: PASS frames=46 samples=7360 homing=6 steady_ehf=4 repeated_speech=20 mismatches=0 handset_audio_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x ROM4 EFR homing decoder complete");
		}
		if (erasure)
		{
			u8 const *const input = memregion("efr_erasure_input")->base();
			unsigned erased = 0;
			for (unsigned f = 0; f < frames; ++f)
			{
				bool const lost = f == 8 || (f >= 32 && f < 35) || (f >= 80 && f < 88) || (f >= 120 && f < 136);
				expect(input[f * 494] == unsigned(lost) && input[f * 494 + 1] == 0 && input[f * 494 + 490] == 0,
					"active speech erasures follow the fixed BFI schedule without SID frames");
				erased += lost;
			}
			expect(erased == 28, "all four active-speech erasure bursts were exercised");
			if (observing)
			{
				osd_printf_info("TMS320C54x ROM4 EFR loss observation replay: COMPLETE frames=163 ram_words=65440 checkpoints=frame_input\n");
				osd_printf_info("TMS320C54x ROM4 EFR loss observation: COMPLETE frames=163 samples=26080 erased=28 bursts=1,3,8,16 mismatches=%u conformance_claim=0 handset_audio_claim=0\n", m_rom4_pcm_mismatches);
				throw emu_fatalerror(0, "TMS320C54x ROM4 EFR loss observation complete");
			}
			osd_printf_info("TMS320C54x ROM4 EFR erasure decoder replay: PASS frames=163 ram_words=65440 checkpoints=frame_input handset_audio_claim=0\n");
			osd_printf_info("TMS320C54x ROM4 EFR erasure decoder: PASS frames=163 samples=26080 erased=28 bursts=1,3,8,16 mismatches=0 handset_audio_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x ROM4 EFR erasure decoder complete");
		}
		if (dtx)
		{
			u8 const *const input = memregion("efr_dtx_input")->base();
			unsigned sid = 0, erased = 0;
			for (unsigned f = 0; f < frames; ++f) { sid += input[f * 494 + 490] == 2; erased += input[f * 494] == 1; }
			expect(sid == 8 && erased == 135 && m_rom4_cni_nonzero_frames == 46, "DTX decoder produces nonzero comfort noise across valid SID and unusable-frame intervals");
			osd_printf_info("TMS320C54x ROM4 EFR DTX decoder replay: PASS frames=518 ram_words=65440 checkpoints=frame_input handset_audio_claim=0\n");
			osd_printf_info("TMS320C54x ROM4 EFR DTX decoder: PASS frames=518 samples=82880 sid=8 erased=135 nonzero_cni_frames=46 mismatches=0 handset_audio_claim=0\n");
			throw emu_fatalerror(0, "TMS320C54x ROM4 EFR DTX decoder complete");
		}
		osd_printf_info("TMS320C54x ROM4 EFR decoder replay: PASS frames=163 ram_words=65440 checkpoints=frame_input handset_audio_claim=0\n");
		osd_printf_info("TMS320C54x ROM4 EFR decoder: PASS frames=163 samples=26080 mismatches=0 handset_audio_claim=0\n");
		throw emu_fatalerror(0, "TMS320C54x ROM4 EFR decoder complete");
	}
	if (m_phase >= 301000 && m_phase < 301831)
	{
		unsigned const vector = (m_phase - 301000) / 3;
		unsigned const stage = (m_phase - 301000) % 3;
		u16 const base = stage ? 0x1600 : 0x1400;
		u16 parameters[57];
		u8 const crc = rom4_efr_parameters(vector, parameters);
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
			m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x0f01 &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1ec4,
			"native EFR packing/unpacking returns with balanced stack");
		if (!stage)
		{
			auto const packed = rom4_efr_packed(vector);
			for (unsigned word = 0; word < packed.size(); ++word)
				expect(data.read_word(0x1500 + word) == packed[word], "native EFR packing matches normative ordering and independent CRC");
		}
		else
		{
			if (stage == 2) parameters[2] ^= 1; // Packed MSB is d30 = s24, protected by CRC8.
			expect(m_cpu->state_int(tms320c54x_device::STATE_A) == (stage == 2 ? 1 : 0),
				"EFR unpacker accepts clean frame and rejects protected-bit corruption");
		}
		for (unsigned p = 0; p < 57; ++p)
			expect(data.read_word(base + p) == parameters[p], "native EFR decoded or preserved parameters");
		expect(data.read_word(base + 57) == crc, "native EFR CRC parameter representation");
		expect(data.read_word(base - 1) == 0x1357 && data.read_word(base + 58) == 0x2468 &&
			data.read_word(0x14ff) == 0x89ab && data.read_word(0x1511) == 0xcdef,
			"native EFR packed and parameter guards");
		if (stage < 2) { start_rom4_efr_packing(vector, stage + 1); return true; }
		if (vector < 276) { start_rom4_efr_packing(vector + 1, 0); return true; }
		osd_printf_info("TMS320C54x ROM4 EFR packing: PASS vectors=277 basis_bits=244 corrupt_frames=277 handset_audio_claim=0\n");
		throw emu_fatalerror(0, "TMS320C54x ROM4 EFR packing complete");
	}
	if (m_phase >= 300000 && m_phase < 300554)
	{
		unsigned const index = m_phase - 300000;
		u16 const base = index & 1 ? 0x1600 : 0x1400;
		u16 const output = base + 0x100;
		u16 parameters[57];
		u8 const expected = rom4_efr_parameters(index / 2, parameters);
		if (data.read_word(output) != expected)
			osd_printf_info("ROM4 EFR CRC mismatch: index=%u actual=%04x expected=%02x pc=%04x\n",
				index, data.read_word(output), expected, unsigned(m_cpu->state_int(tms320c54x_device::STATE_PC)));
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
			m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x0f01 &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1ec4,
			"native EFR CRC returns to sentinel with balanced stack");
		expect(data.read_word(output) == expected, "native EFR CRC matches independent polynomial division");
		for (unsigned p = 0; p < 57; ++p)
			expect(data.read_word(base + p) == parameters[p], "EFR CRC preserves speech parameters");
		expect(data.read_word(base - 1) == 0x1357 && data.read_word(base + 57) == 0x2468 &&
			data.read_word(output - 1) == 0x89ab && data.read_word(output + 1) == 0xcdef,
			"EFR CRC preserves adjacent guards");
		if (index < 553) { start_rom4_efr_crc(index + 1); return true; }
		osd_printf_info("TMS320C54x ROM4 EFR CRC: PASS cases=554 basis_bits=244 protected_bits=65 handset_audio_claim=0\n");
		throw emu_fatalerror(0, "TMS320C54x ROM4 EFR CRC complete");
	}
	if (m_phase >= 71000 && m_phase < 71879)
	{
		unsigned const vector = (m_phase - 71000) / 3;
		unsigned const stage = (m_phase - 71000) % 3;
		auto &data = m_cpu->space(AS_DATA);
		if (!m_cpu->state_int(tms320c54x_device::STATE_IDLE) || m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL))
			osd_printf_info("ROM4 FR packing stop: vector=%u stage=%u pc=%04x illegal=%u sp=%04x\n",
				vector, stage, unsigned(m_cpu->state_int(tms320c54x_device::STATE_PC)),
				unsigned(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL)),
				unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)));
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
			m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x0f01 &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1ec4,
			"native FR packing/CRC/unpacking reaches the return sentinel");
		u16 parameters[76];
		auto const serial = rom4_fr_parameters(vector, parameters);
		u16 packed[12];
		for (unsigned i = 0; i < 12; ++i) packed[i] = data.read_word(0x1500 + i);
		if (stage == 0)
		{
			for (unsigned w = 0; w < 17; ++w)
			{
				u16 expected = 0;
				for (unsigned bit = 0; bit < 16; ++bit)
				{
					u16 const entry = data.read_word(0xb205 + w * 16 + bit);
					expect((entry >> 8) < 76, "FR table parameter offset bound");
					expected = (expected << 1) | BIT(parameters[entry >> 8], 15 - (entry & 15));
				}
				expect(data.read_word(0x1500 + w) == expected, "native FR table packing");
			}
			auto const importance = gsm::tch_f::importance_order(serial);
			u16 const combined = u16((packed[10] >> 9) | (packed[11] << 7));
			for (unsigned j = 0; j < 16; ++j)
			{
				expect(BIT(packed[0], j) == importance[j * 2] &&
					BIT(combined, 15 - j) == importance[j * 2 + 1], "native FR first 32 importance bits");
			}
			for (unsigned j = 0; j < 9; ++j)
				expect(BIT(packed[1], j) == importance[32 + j * 2] &&
					BIT(packed[10], 8 - j) == importance[33 + j * 2], "native FR final 18 protected bits");
		}
		else if (stage == 1)
			expect(m_cpu->state_int(tms320c54x_device::STATE_A) == rom4_crc_expected(packed), "native FR packed frame CRC");
		else
			for (unsigned i = 0; i < 76; ++i)
				expect(data.read_word(0x1400 + i) == parameters[i], "native FR parameter round trip");
		if (stage < 2) { start_rom4_fr_packing(vector, stage + 1); return true; }
		if (vector < 292) { start_rom4_fr_packing(vector + 1, 0); return true; }
		osd_printf_info("TMS320C54x ROM4 FR packing: PASS vectors=293 basis_bits=260 protected_bits=50 speech_claim=0\n");
		throw emu_fatalerror(0, "TMS320C54x ROM4 FR packing complete");
	}
	return false;
}

bool tms320c54x_test_state::check_rom4_crc_input_results()
{
	[[maybe_unused]] auto &program = m_cpu->space(AS_PROGRAM);
	[[maybe_unused]] auto &data = m_cpu->space(AS_DATA);
	if (m_phase >= 64000 && m_phase < 64516)
	{
		unsigned const index = m_phase - 64000;
		u16 words[12];
		rom4_crc_input(index / 2, words);
		u16 const expected = rom4_crc_expected(words);
		if (m_cpu->state_int(tms320c54x_device::STATE_A) != expected)
			osd_printf_info("ROM4 packed CRC mismatch: case=%u actual=%010llx expected=%04x pc=%04x sp=%04x\n",
				index, (unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_A), expected,
				unsigned(m_cpu->state_int(tms320c54x_device::STATE_PC)),
				unsigned(m_cpu->state_int(tms320c54x_device::STATE_SP)));
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
			m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x0f01 &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1ec4 &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == expected,
			"acquired ROM4 packed CRC agrees with independent GSM polynomial division");
		if (index < 515) { start_rom4_crc_packed(index + 1); return true; }
		osd_printf_info("TMS320C54x ROM4 packed CRC: PASS cases=516 basis_bits=192 gsm_crc_claim=1 speech_claim=0\n");
		throw emu_fatalerror(0, "TMS320C54x ROM4 packed CRC complete");
	}
	if (m_phase >= 63000 && m_phase < 63512)
	{
		unsigned const index = m_phase - 63000;
		static constexpr u32 seeds[] = { 0, 1, 7, 0x80000000, 0xffffffff,
			0x12345678, 0xaaaaaaaa, 0x55555555 };
		u32 expected = seeds[index / 64];
		unsigned carry = index & 1;
		for (unsigned bit = 0; bit <= (index / 2) % 32; ++bit)
		{
			unsigned const outgoing = expected & 1;
			expected = (expected >> 1) | (carry << 31);
			if (outgoing) expected ^= 6;
			carry = outgoing;
		}
		expect(!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
			m_cpu->state_int(tms320c54x_device::STATE_IDLE) &&
			m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x0f01 &&
			m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x1ec4 &&
			m_cpu->state_int(tms320c54x_device::STATE_A) == expected &&
			m_cpu->state_int(tms320c54x_device::STATE_B) == 6 &&
			((m_cpu->state_int(tms320c54x_device::STATE_ST0) >> 11) & 1) == carry,
			"acquired ROM4 8179 carry/XOR helper arithmetic and return contract");
		if (index < 511) { start_rom4_crc_helper(index + 1); return true; }
		osd_printf_info("TMS320C54x ROM4 CRC helper: PASS cases=512 gsm_packing_claim=0 speech_claim=0\n");
		throw emu_fatalerror(0, "TMS320C54x ROM4 CRC helper complete");
	}
	if (m_phase == rom4_staged_cold_phase)
	{
		if (m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) || m_cpu->state_int(tms320c54x_device::STATE_IDLE))
		{
			osd_printf_info("TMS320C54x ROM4 staged cold stop: pc=%04x sp=%04x pmst=%04x idle=%u illegal=%u cycles=%llu\n",
				u16(m_cpu->state_int(tms320c54x_device::STATE_PC)), u16(m_cpu->state_int(tms320c54x_device::STATE_SP)),
				u16(m_cpu->state_int(tms320c54x_device::STATE_PMST)), unsigned(m_cpu->state_int(tms320c54x_device::STATE_IDLE)),
				unsigned(m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL)), (unsigned long long)m_cpu->pipeline_cycles());
			expect(false, "Staged ROM4 cold execution stopped before the loader fetch");
		}
		expect(++m_rom4_checks < 10000, "Staged ROM4 cold execution timeout");
		m_check_timer->adjust(attotime::from_usec(100)); return true;
	}
	if (m_phase == 4)
	{
		if (!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
				!m_cpu->state_int(tms320c54x_device::STATE_IDLE))
		{
			expect(++m_rom4_checks < 10000, "ROM4 cold execution timeout");
			m_check_timer->adjust(attotime::from_usec(100));
			return true;
		}
		osd_printf_info("TMS320C54x ROM4 cold frontier: pc=%04x sp=%04x "
				"pmst=%04x idle=%u\n",
				u16(m_cpu->state_int(tms320c54x_device::STATE_PC)),
				u16(m_cpu->state_int(tms320c54x_device::STATE_SP)),
				u16(m_cpu->state_int(tms320c54x_device::STATE_PMST)),
				unsigned(m_cpu->state_int(tms320c54x_device::STATE_IDLE)));
		expect(false, "ROM4 cold execution stopped before the loader-entry fetch");
	}
	if (m_phase == 3)
	{
		if (!m_irq_raised)
		{
			expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE),
					"IDLE3 entry");
			expect(m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x0401,
					"IDLE3 continuation PC");
			m_cpu->set_input_line(2, ASSERT_LINE);
			m_irq_raised = true;
			m_check_timer->adjust(attotime::from_usec(100));
			return true;
		}
		expect(data.read_word(0x0a10) == 0xcafe,
				"maskable interrupt vector execution");
		expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE),
				"fast-interrupt return continuation");
		expect(m_cpu->state_int(tms320c54x_device::STATE_SP) == 0x0300 &&
				!(m_cpu->state_int(tms320c54x_device::STATE_ST1) & 0x0800),
				"RETF restores the fast return and interrupt-mask state");
		expect(m_fast_stack_reads == 0,
				"SPRU172C RETF increments SP without reading the stacked return address");
		osd_printf_info("TMS320C54x fast return case: variant=%u a=%010llx b=%010llx pc=%04x\n",
			m_fast_return_case,
			(unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_A),
			(unsigned long long)m_cpu->state_int(tms320c54x_device::STATE_B),
			unsigned(m_cpu->state_int(tms320c54x_device::STATE_PC)));
		expect(m_cpu->state_int(tms320c54x_device::STATE_PC) == 0x0402 &&
			(!m_fast_return_case || m_cpu->state_int(m_fast_return_case == 1 ?
				tms320c54x_device::STATE_B : tms320c54x_device::STATE_A) == 2),
				"SPRU172C RETFD executes two one-word instructions or one two-word instruction before returning");
		if (m_fast_return_case < 2)
		{
			++m_fast_return_case;
			program.write_word(0x004a, 0xf69b); // RETFD
			program.write_word(0x004b, m_fast_return_case == 1 ? 0xe901 : 0xf020);
			program.write_word(0x004c, m_fast_return_case == 1 ? 0xe902 : 0x0002);
			m_cpu->set_input_line(2, CLEAR_LINE);
			m_irq_raised = false;
			m_fast_stack_reads = 0;
			m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0400);
			m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x0300);
			m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_A, 0);
			m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
			m_check_timer->adjust(attotime::from_usec(100));
			return true;
		}
		osd_printf_info("TMS320C54x fast return bus: PASS cases=3 stack_reads=0 delayed_words=2\n");
		m_cpu->set_input_line(2, CLEAR_LINE);
		m_phase = 1;
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0300);
		m_cpu->set_state_int(tms320c54x_device::STATE_SP, 0x0300);
		m_cpu->set_state_int(tms320c54x_device::STATE_ST1, 0x0100);
		m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		m_check_timer->adjust(attotime::from_usec(100));
		return true;
	}
	if (m_phase == 2)
	{
		if (!m_cpu->state_int(tms320c54x_device::STATE_ILLEGAL) &&
				!m_cpu->state_int(tms320c54x_device::STATE_IDLE))
		{
			expect(++m_rom4_checks < 10000,
					"ROM4 execution frontier timeout");
			m_check_timer->adjust(attotime::from_usec(100));
			return true;
		}
		const u16 pc = m_cpu->state_int(tms320c54x_device::STATE_PC);
		static constexpr u16 expected_response[] = {
			0x3532, 0x0000, 0xffff, 0xffff, 0xff0f, 0x0000, 0x0078,
			0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x087c, 0x0000
		};
		osd_printf_info("TMS320C54x ROM4 execution frontier: pc=%04x "
				"header=%04x,%04x ar2=%04x ar3=%04x\n", pc,
				data.read_word(0x1200), data.read_word(0x1201),
				u16(m_cpu->state_int(tms320c54x_device::STATE_AR2)),
				u16(m_cpu->state_int(tms320c54x_device::STATE_AR3)));
		for (unsigned i = 0; i != std::size(expected_response); ++i)
		{
			const u16 observed = data.read_word(0x1200 + i);
			if (observed != expected_response[i])
				osd_printf_info("TMS320C54x ROM4 response mismatch word=%u actual=%04x expected=%04x\n",
						i, observed, expected_response[i]);
			expect(observed == expected_response[i],
					"complete ROM4 challenge response");
		}
		expect(m_cpu->state_int(tms320c54x_device::STATE_IDLE),
				"ROM4 DSP sleep boundary");
		// IDLE3 at 0x7ec9 advances PC before waiting for a wake source.
		expect(pc == 0x7eca, "ROM4 DSP sleep PC");
		throw emu_fatalerror(0, "TMS320C54x ROM4 frontier complete");
	}
	if (m_phase)
	{
		static constexpr u16 expected[] = {
			0x1cee, 0x7cb6, 0xd2a3, 0xb986, 0x4c57, 0xe65e
		};
		osd_printf_info("TMS320C54x transform result: %04x,%04x,%04x,%04x,%04x,%04x\n",
				data.read_word(0x1202), data.read_word(0x1203),
				data.read_word(0x1204), data.read_word(0x1205),
				data.read_word(0x1206), data.read_word(0x1207));
		for (unsigned i = 0; i != std::size(expected); ++i)
			expect(data.read_word(0x1202 + i) == expected[i],
					"ROM4 challenge transform terminal loop");
		expect(m_cpu->state_int(tms320c54x_device::STATE_BRC) == 0,
				"ROM4 challenge transform repeat count");
		m_phase = 5;
		m_cpu->set_state_int(tms320c54x_device::STATE_PC, 0x0350);
		m_cpu->set_state_int(tms320c54x_device::STATE_AR0, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_ILLEGAL, 0);
		m_cpu->set_state_int(tms320c54x_device::STATE_IDLE, 0);
		m_check_timer->adjust(attotime::from_usec(100));
		return true;
	}
	return false;
}

} // namespace c54x_test
