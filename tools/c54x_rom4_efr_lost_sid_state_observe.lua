-- Read-only semantic boundaries in the isolated lost-SID fixtures.
local cpu = assert(manager.machine.devices[':maincpu'])
local actions = {}
local conditions = {[0xd014] = '(dw@sp==c890)||(dw@sp==c89c)',
    [0xd080] = 'dw@sp==c7fa', [0xc546] = 'dw@sp==c885'}
local function observe(pc, kind, address, count)
    -- Debugger commands accept at most 128 parameters; RAW contains 160 words.
    for first = 0, count - 1, 16 do
        local width = math.min(16, count - first)
        local args = {}
        for i = first, first + width - 1 do args[#args + 1] = string.format('dw@%x', address + i) end
        local prefix = first == 0 and ('efr_boundary: kind=' .. kind .. ' values=') or ''
        local suffix = first + width == count and '\\n' or ''
        actions[pc] = (actions[pc] or '') .. string.format('logerror "%s%s%s",%s;',
            prefix, string.rep('%04x', width), suffix, table.concat(args, ','))
    end
end
local function dynamic_vector(pc, namespace, kind, pointer, count)
    for first = 0, count - 1, 16 do
        local width = math.min(16, count - first)
        local args = {}
        for i = first, first + width - 1 do
            -- Leading digits keep offsets a/b from resolving as accumulators.
            args[#args + 1] = string.format('dw@(%s+%04x)', pointer, i)
        end
        actions[pc] = actions[pc] .. string.format('logerror "%s%s%s",%s;',
            first == 0 and (namespace .. ': kind=' .. kind .. ' values=') or '',
            string.rep('%04x', width), first + width == count and '\\n' or '',
            table.concat(args, ','))
    end
end
cpu.debug:bpset(0xc8de, '1', 'logerror "efr_boundary: kind=entry\\n";g')
observe(0xc7a0, 'R2', 0xb80, 10)
observe(0xc7a0, 'LSF', 0xb94, 10)
observe(0xc809, 'pitch', 0x1303, 1)
observe(0xc85a, 'code', 0x1302, 1)
-- Original decoder return, before the caller's postprocessing.
observe(0xc907, 'RAW', 0x1388, 160)
observe(0xc907, 'LPC', 0x1317, 44)
-- Preserve the full accumulator; debugger shift literals use hexadecimal radix.
cpu.debug:bpset(0xc4d0, '1', 'logerror "efr_gain_policy: previous=%04x gain=%04x candidate_b=%010x\\n",dw@ab9,dw@abe,b;g')
actions[0xd014] = 'logerror "efr_filter_call: return=%04x coefficients=%04x input=%04x output=%04x brc=%04x\\n",dw@sp,ar2,ar3,ar4,brc;'
dynamic_vector(0xd014, 'efr_filter', 'A', 'ar2', 11)
dynamic_vector(0xd014, 'efr_filter', 'EXC', 'ar3', 40)
dynamic_vector(0xd014, 'efr_filter', 'MEM', 'ar4-000a', 10)
-- The total excitation remains in-place while enhancement uses separate scratch.
dynamic_vector(0xd014, 'efr_mixing', 'TOTAL', '(dw@0ac9)+(dw@1312)', 40)
-- ADD $12 after LD #26,DP reads data1312, not the AR2 MMR.
actions[0xc85a] = actions[0xc85a] .. 'logerror "efr_excitation_call: ctrl=%04x base=%04x offset=%04x\\n",dw@09c0,dw@0ac9,dw@1312;'
dynamic_vector(0xc85a, 'efr_excitation', 'FIXED', '0aca', 40)
dynamic_vector(0xc85a, 'efr_excitation', 'ADAPT', '(dw@0ac9)+(dw@1312)', 40)
-- d080 consumes AR0/A as lag/fraction and the 154 preceding excitation words.
actions[0xd080] = 'logerror "efr_predictor_call: return=%04x output=%04x coefficients=%04x lag=%04x fraction=%04x saved_lag=%04x saved_fraction=%04x\\n",dw@sp,ar2,ar3,ar0,a&ffff,dw@130e,dw@1301;'
dynamic_vector(0xd080, 'efr_predictor', 'HISTORY', 'ar2-009a', 154)
-- AR6=1 selects the unsmoothed enhancement branch; postfilter callers differ.
actions[0xc546] = 'logerror "efr_agc_call: return=%04x input=%04x output=%04x mode=%04x\\n",dw@sp,ar2,ar3,ar6;'
dynamic_vector(0xc546, 'efr_agc', 'INPUT', 'ar2', 40)
dynamic_vector(0xc546, 'efr_agc', 'PRE', 'ar3', 40)
actions[0xc885] = ''
dynamic_vector(0xc885, 'efr_agc', 'POST', '0af2', 40)
for pc, action in pairs(actions) do cpu.debug:bpset(pc, conditions[pc] or '1', action .. 'g') end
