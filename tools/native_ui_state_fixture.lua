-- Shared read-only native architecture/I/O replay around emulator save/load.
-- Callers own startup, backed RAM extent, checkpoint and physical contact.
local directory = debug.getinfo(1, 'S').source:sub(2):match('^(.*[/\\])')
local capture = dofile(directory .. 'arm_architecture_snapshot.lua')
return function(config)
    local machine = manager.machine
    local dsp = assert(machine.devices[':dsp_c54x:cpu'])
    local prefix = assert(config.prefix)
    local anchor, phase, saved, complete, reference_endpoint
    local retained, events = {}, {reference = {}, restored = {}}
    local streams,counts,overflow={}, {reference=0,restored=0},false
    local limit=config.io_stream and 1048576 or 16384
    retained[#retained + 1] = dsp.spaces.io:install_write_tap(0, 0xffff, prefix .. '_state_io', function(address, value, mask)
        local now = machine.time:as_double()
        if not anchor or now < anchor or now >= anchor + 2.7 then return end
        if counts[phase]>=limit then overflow=true; return end
        counts[phase]=counts[phase]+1
        local record=string.format('%.9f:%04x:%04x:%04x',now-anchor,address,value,mask)
        if config.io_stream then assert(streams[phase]:write(record,'\n'))
        else events[phase][#events[phase]+1]=record end
    end)
    local function snapshot(label)
        local arm, registers = capture(machine, assert(config.ram_end)), {}
        for _, name in ipairs({'PC','XPC','A','B','T','TRN','RTN','RC','SP','ST0','ST1','PMST',
                'AR0','AR1','AR2','AR3','AR4','AR5','AR6','AR7','BRC','RSA','REA','BK','IFR','IMR',
                'SWWSR','BSCR','SWCR','TIM','PRD','TCR','IDLE','ILLEGAL','GENPC','CURPC'}) do
            registers[#registers + 1] = string.format('%s:%x', name, dsp.state[name].value)
        end
        local data, program = 0, 0
        -- Ordinary DSP RAM only; mapped peripheral registers are excluded.
        for address = 0x80, 0x7fff do data = ((data << 5) - data + dsp.spaces.data:read_u16(address)) & 0xffffffff end
        for address = 0, 0xffff do program = ((program << 5) - program + dsp.spaces.program:read_u16(address)) & 0xffffffff end
        local result = string.format('t=%.9f pc=%08x sp=%08x ram=%08x cpu=%s dsp=%s data=%08x program=%08x',
            arm.time, arm.pc, arm.sp, arm.ram, arm.cpu, table.concat(registers, ','), data, program)
        machine:logerror(prefix .. '_state: phase=' .. label .. ' ' .. result .. '\n')
        return result
    end
    retained[#retained + 1] = emu.add_machine_pre_save_notifier(function()
        saved = snapshot('saved')
        if config.io_stream then streams.reference=assert(io.open(prefix..'_reference.io','wb')) end
        anchor, phase = machine.time:as_double(), 'reference'
    end)
    local function navigate(label)
        local field = assert(config.contact(machine), prefix .. ' physical contact missing')
        field:set_value(1)
        machine:logerror(prefix .. '_state_contact: phase=' .. label .. ' down=1\n')
        assert(emu.wait(.2))
        field:set_value(0)
        machine:logerror(prefix .. '_state_contact: phase=' .. label .. ' down=0\n')
        assert(emu.wait(1))
        machine.screens[':screen']:snapshot(prefix .. '_state_' .. label .. '.png')
        if config.io_stream then streams[label]:close(); streams[label]=nil end
        if config.verify_endpoint then
            local endpoint=snapshot(label..'_end')
            if label=='reference' then reference_endpoint=endpoint
            else assert(endpoint==reference_endpoint,prefix..' replay endpoint differs') end
        end
    end
    retained[#retained + 1] = emu.add_machine_post_load_notifier(function()
        assert(saved and snapshot('restored') == saved, prefix .. ' native architectural restoration differs')
        phase = 'restored'
        if config.io_stream then streams.restored=assert(io.open(prefix..'_restored.io','wb')) end
        local replay = coroutine.create(function()
            assert(emu.wait(1.5))
            navigate('restored')
            assert(not overflow,prefix..' state I/O capture overflow')
            for i=1,config.io_stream and 0 or math.max(#events.reference,#events.restored) do
                if events.reference[i]~=events.restored[i] then
                    machine:logerror(string.format('%s_state_io_divergence: reference=%d restored=%d first=%d\n',
                        prefix,#events.reference,#events.restored,i))
                    for index=i,math.min(i+3,math.max(#events.reference,#events.restored)) do
                        machine:logerror(string.format('%s_state_io_difference: index=%d reference=%s restored=%s\n',
                            prefix,index,events.reference[index] or 'absent',events.restored[index] or 'absent'))
                    end
                    break
                end
            end
            assert(counts.reference>0 and counts.reference==counts.restored,prefix..' native I/O replay count differs')
            if not config.io_stream then
                for i,event in ipairs(events.reference) do
                    assert(events.restored[i]==event,prefix..' native I/O replay differs at '..i)
                end
            end
            machine:logerror(string.format('%s_state_replay: reference=%d restored=%d window=2.7 %s%s\n',
                prefix,counts.reference,counts.restored,config.io_stream and 'streamed=1' or 'exact=1',config.verify_endpoint and ' endpoint=1' or ''))
            complete = true
            machine:logerror(prefix .. '_state: completed=1\n')
        end)
        retained[#retained + 1] = replay
        assert(coroutine.resume(replay))
    end)
    local runner = coroutine.create(function()
        assert(emu.wait(assert(config.checkpoint)))
        machine:save(prefix .. '_native_ui')
        assert(emu.wait(1.5))
        assert(saved, prefix .. ' save did not execute')
        navigate('reference')
        machine:load(prefix .. '_native_ui')
    end)
    retained[#retained + 1] = runner
    retained[#retained + 1] = emu.add_machine_stop_notifier(function()
        for _,stream in pairs(streams) do stream:close() end
        if not complete then machine:logerror(prefix .. '_state: FAIL incomplete\n') end
    end)
    _G[prefix .. '_native_state'] = retained
    assert(coroutine.resume(runner))
end
