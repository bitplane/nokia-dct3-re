-- Cold-start readback using only physical keypad cells and existing NVRAM.
local source = debug.getinfo(1, "S").source:sub(2)
local directory = assert(source:match("^(.*[/])"))
dofile(directory .. "noki6250_runtime_observe.lua")
local pin_enabled = os.getenv('NOKIA_DCT3_6250_PIN_ENTRY') == '1'
if pin_enabled then dofile(directory .. 'noki6250_slow_pin_input.lua') end
local machine = manager.machine
local function cell(column, row)
    for _, field in pairs(machine.ioport.ports[":COL." .. column].fields) do
        if field.mask == (1 << row) then return field end
    end
    error("missing physical matrix cell")
end
local actions = {
    {16, cell(1, 4), 1}, {16.15, cell(1, 4), 0},
    {20, cell(1, 1), 1}, {20.15, cell(1, 1), 0},
    {24, cell(1, 1), 1}, {24.15, cell(1, 1), 0},
    {26, cell(1, 1), 1}, {26.15, cell(1, 1), 0}
}
local deadlines = {17, 19, 22, 25, 27}
if pin_enabled then
    for _, action in ipairs(actions) do action[1] = action[1] + 6 end
    for index, time in ipairs(deadlines) do deadlines[index] = time + 6 end
end
local next_action, next_capture = 1, 1
emu.register_periodic(function()
    local time = machine.time:as_double()
    local action = actions[next_action]
    if action and time >= action[1] then
        action[2]:set_value(action[3])
        machine:logerror(string.format("6250_phonebook_readback_input: step=%d pressed=%d t=%.6f\n",
            next_action, action[3], time))
        next_action = next_action + 1
    end
    if deadlines[next_capture] and time >= deadlines[next_capture] then
        machine.screens[":screen"]:snapshot("6250_phonebook_readback_" .. next_capture .. ".png")
        next_capture = next_capture + 1
    end
end)
