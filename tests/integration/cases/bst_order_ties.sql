# bst index: check sorted output and tie-breaking
CREATE scores 2 int string score name
INSERT INTO scores 6 ROWS
100 Alice
200 Bob
150 Charlie
200 Dave
100 Eve
150 Frank
GENERATE FOR scores bst INDEX ON score
PRINT FROM scores 2 score name WHERE score > 100
PRINT FROM scores 2 score name WHERE score = 150
DELETE FROM scores WHERE name = Dave
PRINT FROM scores 2 score name WHERE score < 100
REMOVE scores
QUIT
