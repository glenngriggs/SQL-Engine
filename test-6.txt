# Invalid commands and proper recovery
CREATE test 2 string int word count
INSERT INTO test 2 ROWS
hello 1
world 2
BANANA
QUACK this is not a real command
NONSENSE yes totally made up
CREATE another 2 int string id tag
INSERT INTO another 1 ROWS
99 alpha
ZEBRA invalid zoo mode
JOIN test AND another WHERE word = tag AND PRINT 2 word 1 tag 2
PRINT FROM test 2 word count ALL
DELETE FROM test WHERE word = hello
BANANAPEEL crash avoid
PRINT FROM test 2 word count ALL
REMOVE another
REMOVE test
QUIT
