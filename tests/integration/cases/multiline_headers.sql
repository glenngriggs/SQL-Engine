# A command's fixed header may span lines; an error in it discards only the rest of the line
# the header ends on, so continuation lines are never misread as new commands.
CREATE t 2 int string a s
INSERT
INTO t 2
ROWS
1 x
2 y
JOIN nope AND t
WHERE a = a AND PRINT 1 a 1
GENERATE FOR nope
hash INDEX ON a
GENERATE FOR t
hash INDEX ON nope
DELETE FROM nope
WHERE a = 1
DELETE FROM t WHERE
nope = 1
INSERT INTO nope 1
ROWS
PRINT FROM nope 1
a ALL
PRINT FROM t
2 s a WHERE
a > 1
JOIN t AND t WHERE a = a
AND PRINT 2 s 1 s 2
GENERATE FOR t
bst INDEX ON s
PRINT FROM t 1 s WHERE s > w
QUIT
