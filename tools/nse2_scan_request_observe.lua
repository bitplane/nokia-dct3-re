-- Attribute the scan extent to its actual host decoder, never change it.
local machine=manager.machine
local cpu=assert(machine.devices[':dsp_c54x:cpu'])
local data=cpu.spaces.data
_G.nse2_scan_request_taps={}
for _,address in ipairs({0x284,0x285}) do
    local count=0
    _G.nse2_scan_request_taps[#_G.nse2_scan_request_taps+1]=data:install_write_tap(address,address,'nse2_scan_extent',function(_,value,mask)
        count=count+1
        if count>16 then return end
        machine:logerror(string.format('nse2_scan_extent: address=%04x value=%04x mask=%04x pc=%04x n=%d t=%.9f observer_writes=0\n',
            address,value,mask,cpu.state['PC'].value,count,machine.time:as_double()))
    end)
end
for _,address in ipairs({0x3d5e,0x3d8f,0x3da3,0x3da5,0x3db1,0x92d}) do
    local count=0
    _G.nse2_scan_request_taps[#_G.nse2_scan_request_taps+1]=cpu.spaces.program:install_read_tap(address,address,'nse2_scan_selector',function()
        if _G.rom4_observation_read then return end
        local pc=cpu.state['PC'].value
        if pc~=address and pc~=address+1 then return end
        count=count+1
        if count>16 then return end
        local pointer=cpu.state['AR2'].value
        machine:logerror(string.format('nse2_scan_selector: address=%04x ar2=%04x input=%04x extent=%04x flags=%04x mode=%04x st0=%04x t=%.9f observer_writes=0\n',
            address,pointer,data:read_u16(pointer),data:read_u16(0x284),data:read_u16(0x6bc),
            data:read_u16(0x195d),cpu.state['ST0'].value,machine.time:as_double()))
    end)
end
