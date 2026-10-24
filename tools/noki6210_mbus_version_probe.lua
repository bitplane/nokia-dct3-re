-- Read-only D1 service-query hypothesis; delivery is not service acceptance.
_G.nokia_mbus_probe_bytes = {0x1f, 0, 0x10, 0xd1, 0, 5, 0, 1, 0, 3, 0, 1, 0xd8}
_G.nokia_mbus_probe_start = 20
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'mbus_input_probe.lua')
