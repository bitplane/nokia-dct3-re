-- Passive program-read/register observations, not instruction-retirement hooks.
local directory=debug.getinfo(1,'S').source:sub(2):match('^(.*[/\\])')
dofile(directory..'nsm1_factory_tone.lua')
local machine=manager.machine
local dsp=machine.devices[':dsp_c54x:cpu']
local taps,counts={},{}
for _,address in ipairs({0x9435,0x3587,0x358a}) do
    counts[address]=0
    taps[#taps+1]=dsp.spaces.program:install_read_tap(address,address,'nsm1_compand_fetch_'..address,function()
        local now=machine.time:as_double()
        if now<86.14 or now>=86.15 then return end
        counts[address]=counts[address]+1
        assert(counts[address]<128,'companding observation cap')
        if counts[address]>32 then return end
        machine:logerror(string.format('nsm1_compand_fetch: address=%04x index=%d pc=%08x a=%010x b=%010x t=%.9f observer_writes=0\n',address,counts[address],dsp.state.PC.value,dsp.state.A.value,dsp.state.B.value,now))
    end)
end
_G.nsm1_compand_fetch_taps=taps
local actions=coroutine.create(function()
    assert(emu.wait(86.14))
    machine:logerror(string.format('nsm1_compand_window: frequency=%04x flags=%04x xpc=%04x t=%.9f observer_writes=0\n',dsp.spaces.data:read_u16(0x857),dsp.spaces.data:read_u16(0x1f2b),dsp.state.XPC.value,machine.time:as_double()))
    assert(emu.wait(.01))
    machine:logerror(string.format('nsm1_compand_summary: input=%d result=%d serial=%d retained=32 observer_writes=0\n',counts[0x9435],counts[0x3587],counts[0x358a]))
end)
assert(coroutine.resume(actions))
