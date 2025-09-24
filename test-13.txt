#DELETE chaos, re-GENERATE
CREATE chaos 3 int int string x y label
INSERT INTO chaos 5 ROWS
1 1 one
2 2 two
3 3 three
4 4 four
5 5 five
GENERATE FOR chaos bst INDEX ON label
DELETE FROM chaos WHERE x < 3
DELETE FROM chaos WHERE y < 4
PRINT FROM chaos 3 x y label ALL
PRINT FROM chaos 3 x y label WHERE label > four
INSERT INTO chaos 3 ROWS
6 6 six
7 7 seven
8 8 eight
PRINT FROM chaos 3 x y label ALL
PRINT FROM chaos 3 x y label WHERE label < seven
DELETE FROM chaos WHERE label > six
PRINT FROM chaos 3 x y label ALL
REMOVE chaos
QUIT
