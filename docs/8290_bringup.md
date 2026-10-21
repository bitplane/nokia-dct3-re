# Nokia 8290 / NSB-7 inputs

## Current boundary

The acquired NSB-7 v5.22 package now has a qualified, contiguous MCU/PPM-A
image. There is no NSB-7 machine configuration or boot acceptance yet.
No PMM, resident DSP mask, hardware profile or provisioning is supplied by
this normalization. The [Nokia 8290 user guide](https://www.instructionsmanuals.com/sites/default/files/2019-05/Nokia-8290-en.pdf)
identifies the product as NSB-7 and GSM1900; the [NSB-7 service manual](https://www.manualslib.com/manual/2171641/Nokia-Nsb-7-Series.html)
is the independent hardware-reference starting point.

## Qualified package contract

`roms/archive-dct3-packages/nsb7_5.22.exe` is a ZIP self-extractor with five
members, not the adjacent-gzip installer format. Its SHA256 is
`5c28267a14e5d3e36ed4dc166987da42e07b04a83ff88499dc38ee4485df0040`.
The own restore INI selects `AMERICA_A`, names both phone streams and gives
erase limits `200000..3cffff`. Both phone headers identify `V  5.22`,
`08-11-01`, `NSB-7`; the PPM identifies pack A.

| Member | CPU start | Bytes | Decoded SHA1 |
| --- | --- | --- | --- |
| `nsb7___5.220` | `200000` | `160000` | `6f2c32bfdb5dce75019f85ac56271fbf05a216cf` |
| `nsb7___5.22a` | `360000` | `70000` | `e09921c52e90888992792eceb57d2241f4eb091c` |

The joined image is `1d0000` bytes, SHA1
`4dacc50f57fe295168a53d3e5855a81acf35ab96`, with no padding or donor tail.

```sh
.venv/bin/python tools/nsb7_package_check.py \
  roms/archive-dct3-packages/nsb7_5.22.exe \
  --output roms/noki8290/8290f522a.fls
```

Next recover the own reset/flash map, GENSIO/CCONT and DSP upload/wait
contracts before selecting a fail-closed machine composition. A sibling's
working HLE or EEPROM is not evidence for those contracts.
