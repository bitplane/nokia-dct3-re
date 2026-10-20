-- Own live-program comparison and native RF source, never a supplied score.
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsm1_native_radio_observe.lua')
local machine=manager.machine
local dsp=assert(machine.devices[':dsp_c54x:cpu'])
local measurements,rf_reads=0,0
_G.nsm1_scan_rf_tap=dsp.spaces.io:install_read_tap(0x27,0x27,'nsm1_scan_rf_input',function(_,value)
    if machine.time:as_double()<50 or rf_reads>=8 then return end
    rf_reads=rf_reads+1
    machine:logerror(string.format('nsm1_scan_rf_input: index=%d value=%04x pc=%04x t=%.9f observer_writes=0\n',
        rf_reads,value,dsp.state['PC'].value,machine.time:as_double()))
end)
_G.nsm1_scan_metric_tap=dsp.spaces.program:install_read_tap(0x0b25,0x0b25,'nsm1_scan_metric',function()
    if machine.time:as_double()<50 or measurements>=8 or dsp.state['PC'].value~=0x0b26 then return end
    measurements=measurements+1
    machine:logerror(string.format('nsm1_scan_metric: index=%d pc=%04x a=%010x threshold=%04x producer=%04x consumer=%04x t=%.9f observer_writes=0\n',
        measurements,dsp.state['PC'].value,dsp.state['A'].value,dsp.spaces.data:read_u16(0x219b),
        dsp.spaces.data:read_u16(0x06b7),dsp.spaces.data:read_u16(0x06b8),machine.time:as_double()))
    if measurements==1 then
        local words={}
        for address=0x1a00,0x1a47 do words[#words+1]=string.format('%04x',dsp.spaces.data:read_u16(address)) end
        machine:logerror('nsm1_scan_rf_queue: start=1a00 words=72 data='..table.concat(words)..' observer_writes=0\n')
    end
end)
local dumped=false
emu.register_frame_done(function()
    if dumped or machine.time:as_double()<45 then return end
    dumped=true
    machine.debugger:command('dasm nsm1_live_measurement.dasm,4701,100,1,:dsp_c54x:cpu')
    machine.debugger:command('dasm nsm1_live_rf_producer.dasm,3300,80,1,:dsp_c54x:cpu')
    machine.debugger:command('dasm nsm1_live_qualification.dasm,b25,40,1,:dsp_c54x:cpu')
end)
