-- Wrap the input exerciser and take MAME's own screen snapshot at NOKIA_DCT3_SNAPSHOT_AT seconds.
local script_dir = (debug.getinfo(1, "S").source:match("^@(.*)/[^/]*$")) or "."
dofile(script_dir .. "/mame_nokia_dct3_input_exerciser.lua")
local machine = manager.machine
local at = tonumber(os.getenv("NOKIA_DCT3_SNAPSHOT_AT") or "16")
local done = false
emu.register_periodic(function()
	if not done and machine.time:as_double() >= at then
		done = true
		machine.video:snapshot()
		machine:logerror(string.format("SNAKESNAP taken at %.3f\n", machine.time:as_double()))
	end
end)
