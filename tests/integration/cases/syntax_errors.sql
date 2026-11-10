# Keywords in the structured part of each command are validated. Each malformed header below
# keeps its full token count: a command reads its whole header before checking it.
CREATE t 1 int a
INSERT TO t 1 ROWS
INSERT INTO t 1 RECORDS
PRINT t 1 a ALL
PRINT FROM t 1 a SOME
DELETE FROM t a = 1
GENERATE ON t hash INDEX ON a
GENERATE FOR t hash IDX ON a
JOIN t OR t WHERE a = a AND PRINT 0
JOIN t AND t WHERE a == a AND PRINT 0
JOIN t AND t WHERE a = a OR PRINT 0
LOAD TO t FROM x.csv
LOAD INTO t AT x.csv
INSERT INTO t 2 ROWS
7
8
PRINT FROM t 1 a ALL
QUIT
