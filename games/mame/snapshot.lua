-- Wrap the upstream harness and take MAME's own screen snapshot at SNAKE_SNAP_AT seconds.
local root = os.getenv("SNAKE_ROOT") or "../.."
dofile(root .. "/mame_nokia_dct3_input_exerciser.lua")
local machine = manager.machine
local at = tonumber(os.getenv("SNAKE_SNAP_AT") or "16")
local done = false
emu.register_periodic(function()
	if not done and machine.time:as_double() >= at then
		done = true
		machine.video:snapshot()
		machine:logerror(string.format("SNAKESNAP taken at %.3f\n", machine.time:as_double()))
	end
end)
