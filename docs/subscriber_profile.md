# Subscriber profile

`gsm::subscriber::profile` is the immutable product-level source for identity
and subscription data shared by the synthetic SIM and GSM network model.  The
default laboratory profile preserves the validated `001-01` behavior exactly.

It owns:

- ICCID and encoded IMSI;
- home LAI and preferred PLMN;
- service-provider name and access-control class; and
- the selected A3/A8 algorithm and test key.

`nokia_dct3_state::apply_product_config` gives the same profile to
`nokia_sim_card_device` and `nokia_gsm_network_device` before either device
initializes persistent state.  The card derives its static EFs and default
cached location from it.  The network derives its home cell identity and
authentication result from it.  Paging continues to use the mobile identity
that the firmware actually registered, rather than rereading a configured
constant.

The profile deliberately does not own mutable card state such as `EF_LOCI`,
`EF_Kc`, ADN or SMS records.  Nor does it own RF and cell scheduling data such
as ARFCN, BSIC or neighbour cells.  Those remain card runtime state and network
environment state respectively.

The default profile is:

- IMSI `001010123456789`, encoded as GSM 11.11 `EF_IMSI`;
- PLMN `001-01`, LAC `1`;
- SPN `DCT3 LAB`, subscriber class `0`; and
- the TS 55.205 AES example A3/A8 profile with key bytes `00` through `0f`.

The authentication, registration and interactive-menu gates prove that this
consolidation does not change the established boot.  Alternate profiles must
be introduced through product configuration and must pass the same consistency
and lifecycle gates; per-device identity overrides are intentionally absent.
