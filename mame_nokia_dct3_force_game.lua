-- Research hook (external Lua only, no driver change): pin game_index_11fd1b to
-- NOKIA_DCT3_FORCE_GAME from NOKIA_DCT3_FORCE_GAME_AT seconds onward, so the next
-- "New game" dispatches through that game_table_2d9484 entry.
local script_dir = (debug.getinfo(1, "S").source:match("^@(.*)/[^/]*$")) or "."
dofile(script_dir .. "/mame_nokia_dct3_input_exerciser.lua")
local machine = manager.machine
local space = machine.devices[":maincpu"].spaces["program"]
local idx = tonumber(os.getenv("NOKIA_DCT3_FORCE_GAME") or "3")
local at = tonumber(os.getenv("NOKIA_DCT3_FORCE_GAME_AT") or "17")
local logged = false
emu.register_periodic(function()
	if machine.time:as_double() >= at then
		space:write_u8(0x11fd1b, idx)
		if not logged then machine:logerror(string.format("FORCEGAME index=%d at %.3f\n", idx, machine.time:as_double())); logged = true end
	end
end)
