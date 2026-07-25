// license:BSD-3-Clause
// copyright-holders:Gaz

#include "nokia_gsm_fr_codec.h"

extern "C"
{
	struct gsm_state;
	gsm_state *gsm_create();
	void gsm_destroy(gsm_state *);
	void gsm_encode(gsm_state *, short *, unsigned char *);
	int gsm_decode(gsm_state *, unsigned char *, short *);
}

static_assert(sizeof(short) == sizeof(std::int16_t));

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
