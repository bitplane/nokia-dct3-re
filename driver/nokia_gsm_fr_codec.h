// license:BSD-3-Clause
// copyright-holders:Gaz

#ifndef MAME_NOKIA_NOKIA_GSM_FR_CODEC_H
#define MAME_NOKIA_NOKIA_GSM_FR_CODEC_H

#include "util/gsmfr.h"

// Keep device snapshot declarations stable while the generic codec utility
// owns the algorithm, framing and receive-side bad-frame substitution.
using nokia_gsm_fr_codec = util::gsm_fr_codec;
using nokia_gsm_fr_receiver = util::gsm_fr_receiver;

#endif // MAME_NOKIA_NOKIA_GSM_FR_CODEC_H
