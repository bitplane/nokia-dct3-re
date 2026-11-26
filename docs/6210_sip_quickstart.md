# 6210 SIP call quick start

How to build only what the native 6210 call needs, check that a machine can
keep up, and run it as a SIP phone. Linux with GCC is assumed.

## Files you supply

Nothing here is distributed. Put these under `roms/`:

| File | SHA-1 |
| --- | --- |
| `noki6210/6210_556c.fls` | `3d9ea319503e78ec69b60d72cda23e461e118ea9` |
| `noki6210/6210 virgin eeprom 005fa000.fls` | `b3a527ede1be87bd715fb3741a81eef5bd422efa` |
| `nsm3r6/nsm3_rom6_program.be16` | `d6b1f40eaa4b2c8a9f7adcb6cb5864d9cc1354fc` |
| `nsm3r6/nsm3_rom6_program_page1.be16` | `51178fc69b3662a8a05291149832f04a1a2c6032` |
| `nsm3r6/nsm3_rom6_data.be16` | `845133638256ed1f07e014437fdee3ececc4508f` |

The three ROM6 files are derived from the captures described under "Nokia
8210 DSP ROM6 reference" in [`roms/README.md`](../roms/README.md):

```sh
mkdir -p roms/nsm3r6
python3 tools/dsp_rom6_candidate_check.py roms/research/nsm3-rom6 --prepare roms/nsm3r6
```

The benchmark and the launcher list any file that is missing or different.

## Build and measure

Install [MAME's build dependencies](https://docs.mamedev.org/initialsetup/compilingmame.html),
then:

```sh
make venv
make build-6210 JOBS=8
make bench-6210-call
```

`build-6210` runs the benchmark itself: the C54x core is compiled
instrumented, trained on the call, then recompiled. `bench-6210-call`
prints the host and the connected call's speed. At 1.0x or more the host can
hold a live call; below that, audio will break up. Please report the whole
output; [`c54x_pipeline_speed.md`](c54x_pipeline_speed.md) has known figures. No host has yet been
measured above 1.0x: a Ryzen 9 9950X3D gives about 1.00x, with no headroom.

## Make a call

```sh
make sip-build
make run-6210-sip SIP_DESTINATION=sip:someone@host
```

Call `sip:dct3@<this host>:25070` and answer on the 6210 with Send (the S
key), or dial any number on the 6210 and press Send to call
`SIP_DESTINATION`; End (E) hangs up. One session takes any number of calls,
one at a time, in either direction.

This is a research composition: the DSP ROM is an 8210's, the cell is a
laboratory one, and the handset's identity records are synthetic.
