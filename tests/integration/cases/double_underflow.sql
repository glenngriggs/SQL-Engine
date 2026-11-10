# A double literal too small to represent reads as (signed) zero, as stream extraction does
CREATE t 2 int double id x
INSERT INTO t 4 ROWS
1 1e-400
2 -1e-400
3 4.9e-324
4 2.5
PRINT FROM t 2 id x ALL
PRINT FROM t 1 id WHERE x = 1e-500
DELETE FROM t WHERE x < 1e-999
PRINT FROM t 2 id x ALL
QUIT
