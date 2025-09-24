# Index rebuild after complete deletion
CREATE rebuilt 2 int string id name
INSERT INTO rebuilt 3 ROWS
1 A
2 B
3 C
GENERATE FOR rebuilt hash INDEX ON id
DELETE FROM rebuilt WHERE id > 0
PRINT FROM rebuilt 2 id name ALL
INSERT INTO rebuilt 2 ROWS
4 D
5 E
PRINT FROM rebuilt 2 id name WHERE id > 3
REMOVE rebuilt
QUIT
