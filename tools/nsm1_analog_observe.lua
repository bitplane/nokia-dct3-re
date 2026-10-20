-- Own NSM-1 shared report-tail and analog validation, observation only.
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsm1_native_observe.lua')
local machine=manager.machine
local cpu=machine.devices[':maincpu']
local memory=cpu.spaces.program
local taps,counts={},{}
for _,address in ipairs({0x223786,0x2239a4,0x2239c2,0x223a68,0x28a884,0x28a892}) do
    counts[address]=0
    taps[#taps+1]=memory:install_read_tap(address & ~3,(address & ~3)+3,'nsm1_analog_'..address,function()
        if cpu.state['PC'].value~=address then return end
        counts[address]=counts[address]+1
        if counts[address]>8 then return end
        local sp=cpu.state['R13'].value
        machine:logerror(string.format('nsm1_analog_entry: pc=%08x index=%d r0=%08x first=%04x second=%04x remaining=%02x gain=%08x offset=%08x t=%.9f observer_writes=0\n',
            address,counts[address],cpu.state['R0'].value,memory:read_u16(sp),memory:read_u16(sp+2),
            memory:read_u8(0x11089a),memory:read_u32(0x11fe3c),memory:read_u32(0x11fe40),machine.time:as_double()))
    end)
end
_G.nsm1_analog_taps=taps
