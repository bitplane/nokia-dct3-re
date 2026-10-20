-- Observe the live measurement return; never supply RF or alter guest state.
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk3_rf_queue_observe.lua')
local machine=manager.machine
local dsp=assert(machine.devices[':dsp_c54x:cpu'])
local count=0
local rf_reads=0
_G.nsk3_scan_rf_tap=dsp.spaces.io:install_read_tap(0x27,0x27,'nsk3_scan_rf_input',function(_,value)
    if machine.time:as_double()<39 or rf_reads>=8 then return end
    rf_reads=rf_reads+1
    machine:logerror(string.format('nsk3_scan_rf_input: index=%d value=%04x pc=%04x t=%.9f observer_writes=0\n',
        rf_reads,value,dsp.state['PC'].value,machine.time:as_double()))
end)
_G.nsk3_scan_metric_tap=dsp.spaces.program:install_read_tap(0x0ae6,0x0ae6,'nsk3_scan_metric',function()
    if machine.time:as_double()<39 or count>=8 or dsp.state['PC'].value~=0x0ae7 then return end
    count=count+1
    machine:logerror(string.format('nsk3_scan_metric: index=%d pc=%04x a=%010x threshold=%04x producer=%04x consumer=%04x t=%.9f observer_writes=0\n',
        count,dsp.state['PC'].value,dsp.state['A'].value,dsp.spaces.data:read_u16(0x219b),
        dsp.spaces.data:read_u16(0x06b7),dsp.spaces.data:read_u16(0x06b8),machine.time:as_double()))
    if count==1 then
        local words={}
        for address=0x1a00,0x1a47 do words[#words+1]=string.format('%04x',dsp.spaces.data:read_u16(address)) end
        machine:logerror('nsk3_scan_rf_queue: start=1a00 words=72 data='..table.concat(words)..' observer_writes=0\n')
    end
end)
local dumped=false
emu.register_frame_done(function()
    if dumped or machine.time:as_double()<45 then return end
    dumped=true
    machine.debugger:command('dasm nsk3_live_measurement.dasm,4701,100,1,:dsp_c54x:cpu')
    machine.debugger:command('dasm nsk3_live_rf_producer.dasm,3300,80,1,:dsp_c54x:cpu')
    machine.debugger:command('dasm nsk3_live_qualification.dasm,ae6,40,1,:dsp_c54x:cpu')
end)
