# Third-party speech codec

The build downloads the official `gsm-1.0.24.tar.gz` release from
<https://www.quut.com/gsm/> and accepts it only when its SHA-256 is
`a3c40c6471928383f4abfcb2e8f24012a1f562be2f17b8d672145d5986681a92`.

This is Jutta Degener and Carsten Bormann's reference implementation of the
ETSI GSM 06.10 full-rate RPE-LTP speech transcoder. It is kept outside the
Nokia device code and wrapped by `driver/nokia_gsm_fr_codec.*`, preserving the
separation between the standardized speech codec and the emulated DSP, radio,
and COBBA devices.

The upstream archive contains its `COPYRIGHT` permission notice. It permits
use, copying, modification, and distribution for any purpose provided that
the notice and attribution are retained. The source archive and extracted
build tree are generated dependencies and are not committed here.
