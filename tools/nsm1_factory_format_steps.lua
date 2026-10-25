-- Read-time pipeline observations; no instruction-retirement claim or writes.
local directory=debug.getinfo(1,'S').source:sub(2):match('^(.*[/\\])')
dofile(directory..'nsm1_factory_click_format.lua')
local machine=manager.machine
local dsp=machine.devices[':dsp_c54x:cpu']
local targets={[0]=true,[2063]=true,[2059]=true,[8254]=true,[8237]=true,[8217]=true,[37738]=true,[0xffffffcb53]=true}
local active,id=nil,0
local taps={}
local function record(address)
    machine:logerror(string.format('nsm1_format_step: id=%d input=%010x address=%04x pc=%08x a=%010x b=%010x t_reg=%04x ar6=%04x st0=%04x st1=%04x time=%.9f observer_writes=0\n',id,active,address,dsp.state.PC.value,dsp.state.A.value,dsp.state.B.value,dsp.state.T.value,dsp.state.AR6.value,dsp.state.ST0.value,dsp.state.ST1.value,machine.time:as_double()))
end
taps[#taps+1]=dsp.spaces.program:install_read_tap(0x9435,0x944b,'format_steps',function(address)
    local now=machine.time:as_double()
    if now<84 or now>=84.25 then return end
    if address==0x9435 then
        local value=dsp.state.A.value
        if targets[value] then
            assert(not active)
            targets[value]=nil
            active=value; id=id+1
        end
    end
    if active~=nil then record(address) end
end)
taps[#taps+1]=dsp.spaces.program:install_read_tap(0x3587,0x358a,'format_return',function(address)
    if active~=nil and (address==0x3587 or address==0x358a) then
        record(address)
        if address==0x358a then active=nil end
    end
end)
_G.nsm1_format_steps=taps
