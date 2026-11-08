-- Raw ADC-pin laboratory experiment. Observes, never changes firmware state.
local directory=assert(debug.getinfo(1,'S').source:sub(2):match('^(.*[/])'))
local machine=manager.machine
local npe3=machine.system.name=='npe3afs' or machine.system.name=='npe3ad0' or machine.system.name=='npe3cp9' or machine.system.name=='npe3zp9' or machine.system.name=='npe3bp9' or machine.system.name=='npe3sp9' or machine.system.name=='npe3tp5' or machine.system.name=='npe3ap5'
dofile(directory..(npe3 and 'rom6_native_radio.lua' or 'rom6_native_observe.lua'))
if npe3 then
    dofile(directory..'rom6_rx_ring_observe.lua')
    if machine.system.name=='npe3ap5' then dofile(directory..'npe3_dedicated_observe.lua') end
    if machine.system.name=='npe3afs' or machine.system.name=='npe3cp9' or machine.system.name=='npe3bp9' or machine.system.name=='npe3sp9' or machine.system.name=='npe3tp5' or machine.system.name=='npe3ap5' then
        dofile(directory..'npe3_sch_provenance.lua')
        dofile(directory..'npe3_fcch_schedule.lua')
        dofile(directory..'npe3_normal_receive.lua')
        if machine.system.name=='npe3bp9' or machine.system.name=='npe3sp9' or machine.system.name=='npe3tp5' or machine.system.name=='npe3ap5' then dofile(directory..'noki6210_plmn_observe.lua') end
        if machine.system.name=='npe3sp9' or machine.system.name=='npe3tp5' or machine.system.name=='npe3ap5' then dofile(directory..'npe3_tx_ring_observe.lua') end
    end
end
local output_cap=npe3 and 496 or 128
local cpu=assert(machine.devices[':dsp_c54x:cpu'])
local results,reads=0,0
local history_writes,filter_writes=0,0
local mixer_writes,mixer_pointer=0,nil
local active_words=0
local handles={}
local function words(first,last)
    local result={}
    for address=first,last do result[#result+1]=string.format('%04x',cpu.spaces.data:read_u16(address)) end
    return table.concat(result,',')
end
if npe3 then
    local windows=0
    handles[#handles+1]=cpu.spaces.data:install_write_tap(0x10,0x10,'npe3_sch_aperture',function(_,value)
        local owner=cpu.state.EXECPC.value
        if value~=0x7737 or owner<0x76a7 or owner>0x76d9 then return end
        windows=windows+1
        if windows~=1 then return end
        machine:logerror(string.format('npe3_sch_aperture: t=%.9f owner=%06x pointer=%04x current=%s next=%s samples=%s observer_writes=0\n',
            machine.time:as_double(),owner,cpu.state.AR3.value,words(0x912,0x913),words(0x91c,0x91d),words(0xd8,0x197)))
    end)
end
handles[#handles+1]=cpu.spaces.io:install_read_tap(0x27,0x27,'rom6_adc_reads',function(_,value)
    reads=reads+1
    if npe3 and active_words<1024 and (value~=0 or active_words>0) then
        active_words=active_words+1
        machine:logerror(string.format('rom6_burst_active_word: sequence=%d read=%d value=%04x t=%.9f observer_writes=0\n',
            active_words,reads,value,machine.time:as_double()))
    end
    if reads==1 then
        machine:logerror(string.format('rom6_adc_first_read: t=%.9f cpu_pc=%06x observer_writes=0\n',machine.time:as_double(),cpu.state.GENPC.value))
        machine:logerror('rom6_adc_processing_initial: history='..words(0x200,0x20f)..' coefficients='..words(0x3ebd,0x3ec4)..' observer_writes=0\n')
        mixer_pointer=cpu.spaces.data:read_u16(0x518)
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
    if filter_writes>output_cap then return end
    machine:logerror(string.format('rom6_adc_filter: write=%d read=%d address=%04x value=%04x cpu_pc=%06x st1=%04x observer_writes=0\n',
        filter_writes,reads,address,value,cpu.state.GENPC.value,cpu.state.ST1.value))
end)
handles[#handles+1]=cpu.spaces.data:install_write_tap(0x518,0x518,'rom6_adc_mixer',function(_,value)
    if reads==0 or reads>1024 then return end
    mixer_writes=mixer_writes+1
    if mixer_writes<=8 then
        local produced={}
        local count=(value-mixer_pointer+0x48)%0x48
        if count==2 or count==4 then
            for index=0,count-1 do
                produced[#produced+1]=string.format('%04x',cpu.spaces.data:read_u16(0x80+(mixer_pointer-0x80+index)%0x48))
            end
        end
        machine:logerror(string.format('rom6_adc_mixer: write=%d read=%d previous=%04x pointer=%04x mode=%04x values=%s cpu_pc=%06x observer_writes=0\n',
            mixer_writes,reads,mixer_pointer,value,cpu.spaces.data:read_u16(0x56e),table.concat(produced,','),cpu.state.GENPC.value))
    end
    mixer_pointer=value
end)
handles[#handles+1]=cpu.spaces.data:install_write_tap(0x022b,0x022b,'rom6_adc_result',function(_,value,mask)
    results=results+1
    if results>32768 then return end
    machine:logerror(string.format('rom6_adc_result_word: sequence=%d t=%.9f value=%04x mode022c=%04x work022a=%04x cpu_pc=%06x mask=%04x observer_writes=0\n',
        results,machine.time:as_double(),value,cpu.spaces.data:read_u16(0x022c),
        cpu.spaces.data:read_u16(0x022a),cpu.state.GENPC.value,mask))
end)
handles[#handles+1]=emu.add_machine_stop_notifier(function()
    if npe3 then
        machine:logerror(string.format('rom6_burst_active_complete: words=%d cap=1024 reads=%d observer_writes=0\n',active_words,reads))
    end
    machine:logerror(string.format('rom6_adc_mixer_complete: writes=%d cap=8 observer_writes=0\n',mixer_writes))
    machine:logerror(string.format('rom6_adc_processing_complete: history_writes=%d filter_writes=%d input_cap=1024 output_cap=%d observer_writes=0\n',history_writes,filter_writes,output_cap))
    machine:logerror(string.format('rom6_adc_lab_complete: result_words=%d cap=32768 reads=%d observer_writes=0\n',results,reads))
end)
_G.rom6_adc_lab=handles
