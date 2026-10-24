-- Own ENVELOPE caller/receive/return boundary, debugger observation only.
local directory = debug.getinfo(1, 'S').source:sub(2):match('^(.*[/\\])')
dofile(directory .. 'nsb3_toolkit_menu_return.lua')
local cpu = manager.machine.devices[':maincpu']
local previous, captures = nil, 0
_G.nsb3_toolkit_envelope_tx = cpu.spaces.program:install_write_tap(0x20034, 0x20037,
    'nsb3_toolkit_envelope_tx', function(address, data, mask)
        for lane = 0, 3 do
            local shift = (3 - lane) * 8
            if address + lane == 0x20036 and mask & (255 << shift) ~= 0 then
                local value = (data >> shift) & 255
                if previous == 0xa0 and value == 0xc2 and captures < 4 then
                    captures = captures + 1
                    local sp, words = cpu.state['R13'].value, {}
                    if sp >= 0x100000 and sp <= 0x10ffc0 then
                        for offset = 0, 60, 4 do
                            words[#words + 1] = string.format('%08x', cpu.spaces.program:read_u32(sp + offset))
                        end
                    end
                    manager.machine:logerror(string.format(
                        'nsb3_toolkit_envelope_tx: pc=%08x lr=%08x sp=%08x stack=%s observer_writes=0\n',
                        cpu.state['R15'].value, cpu.state['R14'].value, sp, table.concat(words, ',')))
                end
                previous = value
            end
        end
    end)
cpu.debug:bpset(0x280878, '1',
    'logerror "nsb3_toolkit_transfer: stage=return source=%08x header=%08x result=%02x observer_writes=0\\n",r6,d@r6,b@(r4+4);g')
