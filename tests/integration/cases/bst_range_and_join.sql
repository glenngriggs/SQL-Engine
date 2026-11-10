# Quiet mode test
CREATE quiet 2 int string id label
INSERT INTO quiet 4 ROWS
1 A
2 B
3 C
4 D
GENERATE FOR quiet bst INDEX ON id
PRINT FROM quiet 1 label WHERE id < 3
DELETE FROM quiet WHERE label = B
JOIN quiet AND quiet WHERE id = id AND PRINT 2 id 1 label 2
REMOVE quiet
QUIT
