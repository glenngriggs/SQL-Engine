# Behaviour fixed relative to the original implementation
CREATE t 2 int string id name
INSERT INTO t 2 ROWS
1 one
2 two
PRINT FROM t 2 id bogus ALL
PRINT FROM t 1 name ALL
CREATE u 2 int string id label
INSERT INTO u 1 ROWS
1 uno
JOIN t AND u WHERE id = id AND PRINT 2 name 1 bogus 2
JOIN t AND u WHERE id = id AND PRINT 2 name 1 label 2
CREATE v 2 int float a b
PRINT FROM v 1 a ALL
CREATE w 2 int int a a
CREATE empty 0
CREATE pairs 2 int string k v
INSERT INTO pairs 3 ROWS
1 a
1 b
2 c
JOIN pairs AND pairs WHERE k = k AND PRINT 2 v 1 v 2
INSERT INTO t 0 ROWS
GENERATE FOR t btree INDEX ON id
JOIN t AND u WHERE id = label AND PRINT 1 id 1
JOIN t AND u WHERE id = id AND PRINT 1 id 3
QUIT
