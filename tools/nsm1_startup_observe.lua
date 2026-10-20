-- Uncapped entry counts after validated storage; no guest writes.
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsm1_native_observe.lua')
local machine=manager.machine
local cpu=machine.devices[':maincpu']
local memory=cpu.spaces.program
local addresses={0x280ac0,0x280b0c,0x2813a0,0x2bf2e6,0x2239f2,0x2bc4d4,0x2af080}
local counts,taps={},{}
for _,address in ipairs(addresses) do
    counts[address]=0
    taps[#taps+1]=memory:install_read_tap(address & ~3,(address & ~3)+3,'nsm1_startup_'..address,function()
        if cpu.state['PC'].value==address then counts[address]=counts[address]+1 end
    end)
end
_G.nsm1_startup_taps=taps
local next_sample=10
emu.register_frame_done(function()
    local now=machine.time:as_double()
    if now<next_sample or next_sample>60 then return end
    local fields={}
    for _,address in ipairs(addresses) do fields[#fields+1]=string.format('%06x:%d',address,counts[address]) end
    machine:logerror(string.format('nsm1_startup_sample: t=%.6f entries=%s reports=%02x charger_state=%02x notification=%02x sim_ready=%02x selector=%02x observer_writes=0\n',
        now,table.concat(fields,','),memory:read_u8(0x1126c1),memory:read_u8(0x1127d8),memory:read_u8(0x1127d9),
        memory:read_u8(0x10e6ce),memory:read_u8(0x10be8c)))
    machine.screens[':screen']:snapshot(string.format('nsm1_startup_%d.png',next_sample))
    next_sample=next_sample+10
end)
