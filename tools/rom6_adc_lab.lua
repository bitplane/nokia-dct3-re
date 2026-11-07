-- Raw ADC-pin laboratory experiment. Observes, never changes firmware state.
local directory=assert(debug.getinfo(1,'S').source:sub(2):match('^(.*[/])'))
dofile(directory..'rom6_native_observe.lua')
local machine=manager.machine
local cpu=assert(machine.devices[':dsp_c54x:cpu'])
local results,reads=0,0
local history_writes,filter_writes=0,0
local handles={}
local function words(first,last)
    local result={}
    for address=first,last do result[#result+1]=string.format('%04x',cpu.spaces.data:read_u16(address)) end
    return table.concat(result,',')
end
handles[#handles+1]=cpu.spaces.io:install_read_tap(0x27,0x27,'rom6_adc_reads',function()
    reads=reads+1
    if reads==1 then
        machine:logerror(string.format('rom6_adc_first_read: t=%.9f cpu_pc=%06x observer_writes=0\n',machine.time:as_double(),cpu.state.GENPC.value))
        machine:logerror('rom6_adc_processing_initial: history='..words(0x200,0x20f)..' coefficients='..words(0x3ebd,0x3ec4)..' observer_writes=0\n')
        local file=assert(io.open('rom6_adc_processing_program.be16','wb'))
        for address=0,0xffff do file:write(string.pack('>I2',cpu.spaces.program:read_u16(address))) end
        file:close()
    end
end)
handles[#handles+1]=cpu.spaces.data:install_write_tap(0x200,0x20f,'rom6_adc_history',function(address,value)
    if reads==0 or reads>1024 then return end
    history_writes=history_writes+1
    machine:logerror(string.format('rom6_adc_history: read=%d address=%04x value=%04x observer_writes=0\n',reads,address,value))
end)
handles[#handles+1]=cpu.spaces.data:install_write_tap(0xc8,0xd7,'rom6_adc_filter',function(address,value)
    if reads==0 or reads>1024 then return end
    filter_writes=filter_writes+1
    if filter_writes>128 then return end
    machine:logerror(string.format('rom6_adc_filter: write=%d read=%d address=%04x value=%04x cpu_pc=%06x st1=%04x observer_writes=0\n',
        filter_writes,reads,address,value,cpu.state.GENPC.value,cpu.state.ST1.value))
end)
handles[#handles+1]=cpu.spaces.data:install_write_tap(0x022b,0x022b,'rom6_adc_result',function(_,value,mask)
    results=results+1
    if results>32768 then return end
    machine:logerror(string.format('rom6_adc_result_word: sequence=%d t=%.9f value=%04x mode022c=%04x work022a=%04x cpu_pc=%06x mask=%04x observer_writes=0\n',
        results,machine.time:as_double(),value,cpu.spaces.data:read_u16(0x022c),
        cpu.spaces.data:read_u16(0x022a),cpu.state.GENPC.value,mask))
end)
handles[#handles+1]=emu.add_machine_stop_notifier(function()
    machine:logerror(string.format('rom6_adc_processing_complete: history_writes=%d filter_writes=%d input_cap=1024 output_cap=128 observer_writes=0\n',history_writes,filter_writes))
    machine:logerror(string.format('rom6_adc_lab_complete: result_words=%d cap=32768 reads=%d observer_writes=0\n',results,reads))
end)
_G.rom6_adc_lab=handles
