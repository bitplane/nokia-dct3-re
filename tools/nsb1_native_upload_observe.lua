-- Observe own program uploads without changing DSP memory or input actions.
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb1_fdn_dial.lua')
local machine=manager.machine
local dsp=machine.devices[':dsp_c54x:cpu']
local writes,highest=0,0
_G.nsb1_native_upload_tap=dsp.spaces.program:install_write_tap(0x2800,0x2fff,'nsb1_native_upload_extent',function(offset,data,mask)
    writes=writes+1
    highest=math.max(highest,offset)
    if offset>=0x29e1 and offset<=0x29e5 then
        machine:logerror(string.format('nsb1_program_extent: address=%04x data=%04x mask=%04x t=%.9f observer_writes=0\n',offset,data,mask,machine.time:as_double()))
    end
end)
local captured=false
emu.register_frame_done(function()
    if captured or machine.time:as_double()<1 then return end
    captured=true
    machine:logerror(string.format('nsb1_program_extent_summary: writes=%d highest=%04x observer_writes=0\n',writes,highest))
    for address=0x29e1,0x29e5 do
        machine:logerror(string.format('nsb1_program_visible: address=%04x program=%04x data=%04x observer_writes=0\n',address,dsp.spaces.program:read_u16(address),dsp.spaces.data:read_u16(address)))
    end
end,'NSB-1 read-only native upload boundary')
