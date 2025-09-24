###
###
CREATE energy 2 bool string on feeling
INSERT INTO energy 7 ROWS
false chill
true nervous
true focused
false sleepy
true exhausted
true buzzed
false excited
GENERATE FOR energy bst INDEX ON on
###
###
###
###
PRINT FROM energy 2 feeling on WHERE on = true
PRINT FROM energy 2 feeling on WHERE on = false
DELETE FROM energy WHERE feeling < energetic
PRINT FROM energy 2 feeling on WHERE on = true
DELETE FROM energy WHERE on > true
####
PRINT FROM energy 2 feeling on WHERE on = true
DELETE FROM energy WHERE on = false
PRINT FROM energy 2 feeling on WHERE on = true
QUIT
