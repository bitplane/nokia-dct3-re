-- RAM probe wrapper for the input exerciser (external Lua only, no driver change).
--   NOKIA_DCT3_RAM_TAP="start:end"        log every write in [start,end] as
--                                         "RAMTAP t=<seconds> addr=<hex> data=<hex> mask=<hex>"
--   NOKIA_DCT3_RAM_POKE="addr:value:at"   from <at> seconds onward, hold byte <value> at <addr>
local script_dir = (debug.getinfo(1, "S").source:match("^@(.*)/[^/]*$")) or "."
dofile(script_dir .. "/mame_nokia_dct3_input_exerciser.lua")
local machine = manager.machine
local space = machine.devices[":maincpu"].spaces["program"]
-- Kept global: a tap handle that is garbage-collected is silently removed.
nokia_dct3_ram_probe_taps = {}
local taps = nokia_dct3_ram_probe_taps
local tap = os.getenv("NOKIA_DCT3_RAM_TAP")
if tap then
	local a, b = tap:match("([%w]+):([%w]+)")
	a, b = tonumber(a), tonumber(b)
	taps[#taps + 1] = space:install_write_tap(a & ~1, b | 1, "nokia_dct3_ram_probe", function(offset, data, mask)
		machine:logerror(string.format("RAMTAP t=%.6f addr=%06x data=%04x mask=%04x\n",
				machine.time:as_double(), offset, data, mask))
		return data
	end)
end
local poke = os.getenv("NOKIA_DCT3_RAM_POKE")
if poke then
	local addr, value, at = poke:match("([%w]+):([%w]+):([%d%.]+)")
	addr, value, at = tonumber(addr), tonumber(value), tonumber(at)
	emu.register_periodic(function()
		if machine.time:as_double() >= at then space:write_u8(addr, value) end
	end)
end
