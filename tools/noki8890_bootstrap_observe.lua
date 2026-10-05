-- Passive NSB-6 bootstrap access census; never supplies DSP values.
local source = debug.getinfo(1, "S").source:sub(2)
dofile(assert(source:match("^(.*[/])")) .. "dct3_model_scout.lua")
local machine = manager.machine
local cpu = assert(machine.devices[":maincpu"])
local memory = cpu.spaces["program"]
local seen = {}
local tap = memory:install_read_tap(0x10000, 0x10103,
    "8890_bootstrap_read", function(address, data, mask)
        local pc = cpu.state["PC"].value
        if address > 0x10008 and address < 0x100fc then return end
        local key = string.format('%x:%x:%x', pc, address, mask)
        if seen[key] then return end
        seen[key] = true
        machine:logerror(string.format(
            "8890_bootstrap_read: pc=%08x address=%08x data=%08x mask=%08x t=%.6f\n",
            pc, address, data, mask, machine.time:as_double()))
    end)
_G.noki8890_bootstrap_tap = tap
assert(cpu.debug, "8890 service observation requires the MAME debugger")
cpu.debug:bpset(0x2c3398, 'temp7<24',
    'temp7=temp7+1;logerror "8890_service_control: command=%04x argument=%04x commit=%04x pending_address=%08x\\n",r4,r6,r5,r3+r8;g')
cpu.debug:bpset(0x2c307c, 'temp8<24',
    'temp8=temp8+1;logerror "8890_service_encoder: command=%04x argument=%04x commit=%04x caller=%08x\\n",r0,r1,r2,r14;g')
cpu.debug:bpset(0x2409ac, 'temp9<8',
    'temp9=temp9+1;logerror "8890_selftest_reply: command=%02x faults=%02x flag=%02x\\n",b@(r4+8),b@(r4+9),b@13fde1;g')
cpu.debug:bpset(0x240a60, 'temp6<16',
    'temp6=temp6+1;logerror "8890_service_return: command=%02x faults=%02x/%02x/%02x\\n",b@(r4+8),b@13fbef,b@13fbf0,b@13fbf1;g')
