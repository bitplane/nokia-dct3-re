-- Research hook: pin game_index_11fd1b to SNAKE_FORCE_GAME from SNAKE_FORCE_AT
-- seconds onward, so the next "New game" dispatches through that plugin-table
-- entry. Wraps the upstream harness for keys and LCD frames. Not a driver change.
local root = os.getenv("SNAKE_ROOT") or "../.."
dofile(root .. "/mame_nokia_dct3_input_exerciser.lua")
local machine = manager.machine
local space = machine.devices[":maincpu"].spaces["program"]
local idx = tonumber(os.getenv("SNAKE_FORCE_GAME") or "3")
local at = tonumber(os.getenv("SNAKE_FORCE_AT") or "17")
local logged = false
emu.register_periodic(function()
	if machine.time:as_double() >= at then
		space:write_u8(0x11fd1b, idx)
		if not logged then machine:logerror(string.format("FORCEGAME index=%d at %.3f\n", idx, machine.time:as_double())); logged = true end
	end
end)
