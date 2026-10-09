-- Own NPE-3 physical registration only, leaving the subscription active.
_G.dct3_divert_sequences = {'*21*5551234#'}
local directory = assert(debug.getinfo(1, 'S').source:match('^@(.*/)'))
dofile(directory .. 'noki6210_divert_lifecycle_input.lua')
