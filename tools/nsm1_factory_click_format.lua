-- Passive formatter census during an organic menu click; no guest writes.
local directory=debug.getinfo(1,'S').source:sub(2):match('^(.*[/\\])')
dofile(directory..'nsm1_factory_tone.lua')
local machine=manager.machine
local dsp=machine.devices[':dsp_c54x:cpu']
local taps,counts={},{}
for _,address in ipairs({0x9435,0x3587,0x358a}) do
    counts[address]=0
    taps[#taps+1]=dsp.spaces.program:install_read_tap(address,address,'click_format_'..address,function()
        local now=machine.time:as_double()
        if now<84 or now>=84.25 then return end
        counts[address]=counts[address]+1
        assert(counts[address]<=2000,'click formatter cap')
        machine:logerror(string.format('nsm1_click_format: address=%04x index=%d a=%010x b=%010x t=%.9f observer_writes=0\n',address,counts[address],dsp.state.A.value,dsp.state.B.value,now))
    end)
end
_G.nsm1_click_format=taps
local actions=coroutine.create(function()
    dofile(directory..'fixture_deadline.lua')(84.25)
    machine:logerror(string.format('nsm1_click_summary: input=%d result=%d serial=%d observer_writes=0\n',counts[0x9435],counts[0x3587],counts[0x358a]))
end)
assert(coroutine.resume(actions))
