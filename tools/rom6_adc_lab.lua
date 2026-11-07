-- Raw ADC-pin laboratory experiment. Observes, never changes firmware state.
local directory=assert(debug.getinfo(1,'S').source:sub(2):match('^(.*[/])'))
dofile(directory..'rom6_native_observe.lua')
local machine=manager.machine
local cpu=assert(machine.devices[':dsp_c54x:cpu'])
local results,reads=0,0
local handles={}
handles[#handles+1]=cpu.spaces.io:install_read_tap(0x27,0x27,'rom6_adc_reads',function() reads=reads+1 end)
handles[#handles+1]=cpu.spaces.data:install_write_tap(0x022b,0x022b,'rom6_adc_result',function(_,value,mask)
    results=results+1
    if results>32768 then return end
    machine:logerror(string.format('rom6_adc_result_word: sequence=%d t=%.9f value=%04x mode022c=%04x work022a=%04x cpu_pc=%06x mask=%04x observer_writes=0\n',
        results,machine.time:as_double(),value,cpu.spaces.data:read_u16(0x022c),
        cpu.spaces.data:read_u16(0x022a),cpu.state.GENPC.value,mask))
end)
handles[#handles+1]=emu.add_machine_stop_notifier(function()
    machine:logerror(string.format('rom6_adc_lab_complete: result_words=%d cap=32768 reads=%d observer_writes=0\n',results,reads))
end)
_G.rom6_adc_lab=handles
