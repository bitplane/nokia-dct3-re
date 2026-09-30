-- Function-entry coverage per phase for the Nokia 3210 in MAME.
--
-- Wraps the upstream input exerciser (keys, LCD mirrors, boot summary) and adds
-- a breakpoint at every candidate Thumb function entry. Each breakpoint's action
-- logs "COV <pc>" to error.log once, disables itself and continues. At each
-- phase boundary every breakpoint is re-enabled, so error.log carries, per
-- phase, the set of functions entered at least once.
--
-- Requires: -debug -debugger none -log
-- Env: SNAKE_ROOT       repository root (absolute)
--      SNAKE_COV_ENTRIES file with one "hexaddr tag" per line (default games/data/entry_candidates.txt)
--      SNAKE_COV_PHASES  "name:seconds,name:seconds,..." absolute emulation seconds at which
--                        a new phase starts (phase "boot" starts at 0)
local root = os.getenv("SNAKE_ROOT") or "../.."
dofile(root .. "/mame_nokia_dct3_input_exerciser.lua")

local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local dbg = cpu.debug
assert(dbg, "coverage.lua needs -debug (device_debug unavailable)")

local entries_path = os.getenv("SNAKE_COV_ENTRIES") or (root .. "/games/data/entry_candidates.txt")
local count, mismatch = 0, 0
-- learn the next breakpoint index
local probe = dbg:bpset(0x200040, "1", "")
local expect = probe + 1
dbg:bpclear(probe)
for line in io.lines(entries_path) do
	local hex = line:match("^%s*([0-9a-fA-F]+)")
	if hex then
		local addr = tonumber(hex, 16) & ~1
		local action = string.format("logerror \"COV %%x\\n\",pc;bpdisable %x;g", expect)
		local got = dbg:bpset(addr, "1", action)
		if got ~= expect then mismatch = mismatch + 1 end
		expect = got + 1
		count = count + 1
	end
end
machine:logerror(string.format("COVSETUP breakpoints=%d index_mismatch=%d\n", count, mismatch))

local phases = {}
for name, secs in string.gmatch(os.getenv("SNAKE_COV_PHASES") or "", "([%w_]+):([%d%.]+)") do
	phases[#phases + 1] = { name = name, at = tonumber(secs) }
end
table.sort(phases, function(a, b) return a.at < b.at end)
machine:logerror("COVPHASE boot 0.000\n")

local function seconds()
	return machine.time:as_double()
end

local next_phase = 1
emu.register_periodic(function()
	local now = seconds()
	while next_phase <= #phases and now >= phases[next_phase].at do
		dbg:bpenable()
		machine:logerror(string.format("COVPHASE %s %.3f\n", phases[next_phase].name, now))
		next_phase = next_phase + 1
	end
end)

-- Optional raw PC trace window: SNAKE_TRACE="start_seconds:stop_seconds:file"
local trace_spec = os.getenv("SNAKE_TRACE")
if trace_spec then
	local t0, t1, file = trace_spec:match("([%d%.]+):([%d%.]+):(.+)")
	t0, t1 = tonumber(t0), tonumber(t1)
	local state = 0
	emu.register_periodic(function()
		local now = seconds()
		if state == 0 and now >= t0 then
			machine.debugger:command(string.format("trace %s,0,noloop", file))
			machine:logerror(string.format("COVTRACE start %.3f %s\n", now, file))
			state = 1
		elseif state == 1 and now >= t1 then
			machine.debugger:command("trace off")
			machine:logerror(string.format("COVTRACE stop %.3f\n", now))
			state = 2
		end
	end)
end
