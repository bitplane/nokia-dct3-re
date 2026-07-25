// license:BSD-3-Clause
// copyright-holders:Gaz

#include "nokia_gsm_fr_codec.h"

#include <algorithm>
#include <iterator>
#include <limits>

extern "C"
{
	// The linked, pinned libgsm 1.0.24 exposes gsm_state only in private.h.
	// Repeat that stable algorithmic layout here so snapshots can name every
	// predictor value without serializing pointers or ABI padding.
	struct gsm_state
	{
		short dp0[280];
		short e[50];
		short z1;
		long l_z2;
		int mp;
		short u[8];
		short larpp[2][8];
		short j;
		short ltp_cut;
		short nrp;
		short v[9];
		short msr;
		char verbose;
		char fast;
		char wav_fmt;
		unsigned char frame_index;
		unsigned char frame_chain;
	};
	gsm_state *gsm_create();
	void gsm_destroy(gsm_state *);
	void gsm_encode(gsm_state *, short *, unsigned char *);
	int gsm_decode(gsm_state *, unsigned char *, short *);
}

static_assert(sizeof(short) == sizeof(std::int16_t));
static_assert(sizeof(int) == sizeof(std::int32_t));
static_assert(sizeof(long) <= sizeof(std::int64_t));

namespace {

nokia_gsm_fr_codec::channel_state export_state(const gsm_state &source)
{
	nokia_gsm_fr_codec::channel_state result{};
	std::copy(std::begin(source.dp0), std::end(source.dp0), result.dp0.begin());
	std::copy(std::begin(source.e), std::end(source.e), result.e.begin());
	result.z1 = source.z1;
	result.l_z2 = source.l_z2;
	result.mp = source.mp;
	std::copy(std::begin(source.u), std::end(source.u), result.u.begin());
	for (unsigned bank = 0; bank < 2; ++bank)
		std::copy(std::begin(source.larpp[bank]),
				std::end(source.larpp[bank]),
				result.larpp.begin() + bank * 8);
	result.j = source.j;
	result.ltp_cut = source.ltp_cut;
	result.nrp = source.nrp;
	std::copy(std::begin(source.v), std::end(source.v), result.v.begin());
	result.msr = source.msr;
	result.verbose = source.verbose;
	result.fast = source.fast;
	result.wav_fmt = source.wav_fmt;
	result.frame_index = source.frame_index;
	result.frame_chain = source.frame_chain;
	return result;
}

bool import_state(
		const nokia_gsm_fr_codec::channel_state &source, gsm_state &result)
{
	// nrp is a 40..120 decoder lag and j selects one of two LAR histories.
	// Reject malformed external state rather than indexing libgsm out of range.
	if (source.j < 0 || source.j > 1 ||
			source.nrp < 40 || source.nrp > 120 ||
			source.l_z2 < std::numeric_limits<long>::min() ||
			source.l_z2 > std::numeric_limits<long>::max())
		return false;

	std::copy(source.dp0.begin(), source.dp0.end(), std::begin(result.dp0));
	std::copy(source.e.begin(), source.e.end(), std::begin(result.e));
	result.z1 = source.z1;
	result.l_z2 = long(source.l_z2);
	result.mp = source.mp;
	std::copy(source.u.begin(), source.u.end(), std::begin(result.u));
	for (unsigned bank = 0; bank < 2; ++bank)
		std::copy(source.larpp.begin() + bank * 8,
				source.larpp.begin() + (bank + 1) * 8,
				std::begin(result.larpp[bank]));
	result.j = source.j;
	result.ltp_cut = source.ltp_cut;
	result.nrp = source.nrp;
	std::copy(source.v.begin(), source.v.end(), std::begin(result.v));
	result.msr = source.msr;
	result.verbose = source.verbose;
	result.fast = source.fast;
	result.wav_fmt = source.wav_fmt;
	result.frame_index = source.frame_index;
	result.frame_chain = source.frame_chain;
	return true;
}

} // anonymous namespace

nokia_gsm_fr_codec::nokia_gsm_fr_codec()
{
	reset();
}

nokia_gsm_fr_codec::~nokia_gsm_fr_codec()
{
	release();
}

void nokia_gsm_fr_codec::release()
{
	if (m_encoder)
		gsm_destroy(static_cast<gsm_state *>(m_encoder));
	if (m_decoder)
		gsm_destroy(static_cast<gsm_state *>(m_decoder));
	m_encoder = nullptr;
	m_decoder = nullptr;
}

void nokia_gsm_fr_codec::reset()
{
	release();
	m_encoder = gsm_create();
	m_decoder = gsm_create();
	if (!available())
		release();
}

bool nokia_gsm_fr_codec::encode(const pcm_block &pcm, speech_frame &frame)
{
	if (!m_encoder)
		return false;
	// libgsm predates const-correct interfaces but does not modify input.
	gsm_encode(static_cast<gsm_state *>(m_encoder),
			const_cast<short *>(reinterpret_cast<const short *>(pcm.data())),
			frame.data());
	return true;
}

bool nokia_gsm_fr_codec::decode(const speech_frame &frame, pcm_block &pcm)
{
	if (!m_decoder)
		return false;
	return gsm_decode(static_cast<gsm_state *>(m_decoder),
			const_cast<unsigned char *>(frame.data()),
			reinterpret_cast<short *>(pcm.data())) == 0;
}

nokia_gsm_fr_codec::state nokia_gsm_fr_codec::snapshot() const
{
	state result{};
	if (m_encoder)
		result.channels[0] =
				export_state(*static_cast<const gsm_state *>(m_encoder));
	if (m_decoder)
		result.channels[1] =
				export_state(*static_cast<const gsm_state *>(m_decoder));
	return result;
}

bool nokia_gsm_fr_codec::restore(const state &state)
{
	if (!available())
		return false;

	// Validate both directions before changing either one.
	gsm_state encoder = *static_cast<gsm_state *>(m_encoder);
	gsm_state decoder = *static_cast<gsm_state *>(m_decoder);
	if (!import_state(state.channels[0], encoder) ||
			!import_state(state.channels[1], decoder))
		return false;
	*static_cast<gsm_state *>(m_encoder) = encoder;
	*static_cast<gsm_state *>(m_decoder) = decoder;
	return true;
}
