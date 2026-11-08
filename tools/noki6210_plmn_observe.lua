-- Own NPE-3 v5.56 bounded SI3 observations; no state or transport writes.
local machine = manager.machine
assert(machine.system.name == 'npe3hle' or machine.system.name == 'npe3bp9' or machine.system.name == 'npe3sp9' or machine.system.name=='npe3tp5',
    'requires own NPE-3 HLE or raw-BCCH laboratory composition')
local cpu = assert(machine.devices[':maincpu'])
local space = cpu.spaces['program']
local taps, busy = {}, false
for _, pc in ipairs({0x3b9e34, 0x3b9e84, 0x3b9e8e, 0x3b9e98, 0x3ba0fa,
                    0x3b9fd0, 0x3ba0c0, 0x3ba060, 0x3b9ed4, 0x3b9a72, 0x3b9a82, 0x3ba0d6,
                    0x3b9fe0, 0x3b9ff0, 0x3ba002, 0x3b9764,
                    0x2d3de6, 0x2d3e1a, 0x2d3e3c, 0x3c915a, 0x4b2df2, 0x4b2e0c,
                    0x2d3ec0, 0x2d3f26, 0x2d3f44, 0x2d3f56, 0x2d3f64,
                    0x2d3f6e, 0x2d3930, 0x2d3b16}) do
    local hits = 0
    taps[#taps + 1] = space:install_read_tap(pc & ~3, (pc & ~3) + 3,
        'plmn_parse_' .. pc, function(address)
            if busy or address ~= (pc & ~3) or cpu.state['PC'].value ~= pc or hits >= 12 then return end
            hits = hits + 1
            busy = true
            local owner = cpu.state['R5'].value
            local selection = owner >= 0x100000 and owner + 8 < 0x180000
                and space:read_u8(owner + 8) or 0xff
            local header={}
            if owner>=0x100000 and owner+24<=0x180000 then
                for at=owner,owner+23 do header[#header+1]=string.format('%02x',space:read_u8(at)) end
            end
            machine:logerror(string.format(
                'plmn_si3_parse: pc=%08x hit=%d r0=%08x state=%04x flags=%02x owner=%08x selection=%02x t=%.9f\n',
                pc, hits, cpu.state['R0'].value, space:read_u16(0x16b220),
                space:read_u8(0x16b209), owner, selection, machine.time:as_double()))
            busy = false
            if pc==0x3b9e84 or pc==0x3b9e8e or pc==0x3b9e98 or pc==0x3ba0fa then
                machine:logerror(string.format('plmn_si3_admission: pc=%08x hit=%d channel=%08x r0=%08x r1=%08x block=%08x bytes=%s t=%.9f observer_writes=0\n',
                    pc,hits,cpu.state['R8'].value,cpu.state['R0'].value,cpu.state['R1'].value,owner,table.concat(header),machine.time:as_double()))
            end
        end)
end
_G.plmn_parser_observation = {taps = taps}
